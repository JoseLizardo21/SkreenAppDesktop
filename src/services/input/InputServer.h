#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <cstdint>

// Callback: (type, slot, nx, ny) where nx/ny are normalized 0.0-1.0
using InputEventCallback = std::function<void(uint8_t, int32_t, float, float)>;

class InputServer {
public:
    InputServer();
    ~InputServer();
    void start(InputEventCallback cb);
    void stop();
private:
    int server_fd_{-1};
    std::atomic<bool> running_{false};
    std::thread thread_;
    InputEventCallback callback_;
    void serverLoop();
};
