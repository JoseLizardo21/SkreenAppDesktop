#pragma once
#include <string>
#include <thread>
#include <atomic>

class NotifyServer {
public:
    NotifyServer();
    ~NotifyServer();
    void start();
    void stop();
    void send(const std::string& message);
private:
    int server_fd_{-1};
    std::atomic<int> client_fd_{-1};
    std::atomic<bool> running_{false};
    std::thread thread_;
    void acceptLoop();
};
