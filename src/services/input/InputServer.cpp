#include "InputServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

InputServer::InputServer() = default;

InputServer::~InputServer() {
    stop();
    if (thread_.joinable()) thread_.join();
}

void InputServer::start(InputEventCallback cb) {
    callback_ = std::move(cb);
    running_ = true;
    thread_ = std::thread(&InputServer::serverLoop, this);
}

void InputServer::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }
}

// Parse big-endian int32
static int32_t readBEInt32(const uint8_t* buf) {
    return (int32_t)(((uint32_t)buf[0] << 24) |
                     ((uint32_t)buf[1] << 16) |
                     ((uint32_t)buf[2] <<  8) |
                     ((uint32_t)buf[3]));
}

// Parse big-endian float32
static float readBEFloat32(const uint8_t* buf) {
    uint32_t bits = ((uint32_t)buf[0] << 24) |
                    ((uint32_t)buf[1] << 16) |
                    ((uint32_t)buf[2] <<  8) |
                    ((uint32_t)buf[3]);
    float val;
    memcpy(&val, &bits, sizeof(float));
    return val;
}

void InputServer::serverLoop() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[InputServer] Failed to create socket\n";
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(9003);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[InputServer] bind() failed on port 9003\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    listen(server_fd_, 5);
    std::cout << "[InputServer] Listening on port 9003\n";

    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &len);
        if (client_fd < 0) {
            if (running_)
                std::cerr << "[InputServer] accept() error: " << strerror(errno) << "\n";
            break;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        std::cout << "[InputServer] Client connected: " << ip << "\n";

        int flag = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        // Read 13-byte packets until client disconnects
        uint8_t buf[13];
        while (running_) {
            ssize_t total = 0;
            while (total < 13) {
                ssize_t n = recv(client_fd, buf + total, 13 - total, 0);
                if (n <= 0) goto client_done;
                total += n;
            }

            {
                uint8_t  type = buf[0];
                int32_t  slot = readBEInt32(buf + 1);
                float    nx   = readBEFloat32(buf + 5);
                float    ny   = readBEFloat32(buf + 9);

                if (callback_) callback_(type, slot, nx, ny);
            }
        }

    client_done:
        std::cout << "[InputServer] Client disconnected: " << ip << "\n";
        close(client_fd);
    }

    std::cout << "[InputServer] Server stopped\n";
}
