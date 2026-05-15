#include "NotifyServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

NotifyServer::NotifyServer() = default;

NotifyServer::~NotifyServer() {
    stop();
    if (thread_.joinable()) thread_.join();
}

void NotifyServer::start() {
    running_ = true;
    thread_ = std::thread(&NotifyServer::acceptLoop, this);
}

void NotifyServer::stop() {
    running_ = false;
    int cfd = client_fd_.load();
    if (cfd >= 0) { shutdown(cfd, SHUT_RDWR); close(cfd); client_fd_ = -1; }
    if (server_fd_ >= 0) { shutdown(server_fd_, SHUT_RDWR); close(server_fd_); server_fd_ = -1; }
}

void NotifyServer::send(const std::string& message) {
    int cfd = client_fd_.load();
    if (cfd < 0) return;
    ::send(cfd, message.c_str(), message.size(), MSG_NOSIGNAL);
}

void NotifyServer::acceptLoop() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[NotifyServer] Failed to create socket\n";
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(9004);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[NotifyServer] bind() failed on port 9004\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    listen(server_fd_, 5);
    std::cout << "[NotifyServer] Listening on port 9004\n";

    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int cfd = accept(server_fd_, (struct sockaddr*)&client_addr, &len);
        if (cfd < 0) {
            if (running_) std::cerr << "[NotifyServer] accept() error: " << strerror(errno) << "\n";
            break;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        std::cout << "[NotifyServer] Client connected: " << ip << "\n";

        client_fd_ = cfd;

        // Hold connection open; detect client disconnection via recv
        char buf[1];
        while (running_) {
            ssize_t n = recv(cfd, buf, 1, 0);
            if (n <= 0) break;
        }

        client_fd_ = -1;
        close(cfd);
        std::cout << "[NotifyServer] Client disconnected\n";
    }
}
