#include "WebRtcSignaling.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

WebRtcSignaling::WebRtcSignaling() = default;

WebRtcSignaling::~WebRtcSignaling() {
    stop();
    if (thread_.joinable()) thread_.join();
}

void WebRtcSignaling::start() {
    running_ = true;
    thread_ = std::thread(&WebRtcSignaling::acceptLoop, this);
}

void WebRtcSignaling::stop() {
    running_ = false;
    int cfd = client_fd_.load();
    if (cfd >= 0) { shutdown(cfd, SHUT_RDWR); close(cfd); client_fd_ = -1; }
    if (server_fd_ >= 0) { shutdown(server_fd_, SHUT_RDWR); close(server_fd_); server_fd_ = -1; }
}

void WebRtcSignaling::send(const std::string& message) {
    int cfd = client_fd_.load();
    if (cfd < 0) return;
    ::send(cfd, message.c_str(), message.size(), MSG_NOSIGNAL);
}

void WebRtcSignaling::acceptLoop() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[WebRtcSignaling] Failed to create socket\n";
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(9002);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[WebRtcSignaling] bind() failed on port 9002\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    listen(server_fd_, 5);
    std::cout << "[WebRtcSignaling] Listening on port 9002\n";

    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int cfd = accept(server_fd_, (struct sockaddr*)&client_addr, &len);
        if (cfd < 0) {
            if (running_) std::cerr << "[WebRtcSignaling] accept() error: " << strerror(errno) << "\n";
            break;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        std::cout << "[WebRtcSignaling] Client connected: " << ip << "\n";

        int flag = 1;
        setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        client_fd_ = cfd;
        if (on_client_connected_) on_client_connected_();

        // Lee mensajes JSON delimitados por '\n' hasta que el cliente se desconecte
        std::string buffer;
        char chunk[4096];
        while (running_) {
            ssize_t n = recv(cfd, chunk, sizeof(chunk), 0);
            if (n <= 0) break;
            buffer.append(chunk, n);

            size_t pos;
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);
                if (!line.empty() && on_message_) on_message_(line);
            }
        }

        client_fd_ = -1;
        close(cfd);
        std::cout << "[WebRtcSignaling] Client disconnected\n";

        // Si seguimos corriendo (no es un stop()), refrescar el pipeline para que
        // la próxima conexión negocie WebRTC sobre un webrtcbin nuevo
        if (running_ && on_client_disconnected_)
            on_client_disconnected_();
    }
}
