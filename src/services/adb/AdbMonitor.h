#ifndef ADB_MONITOR_H
#define ADB_MONITOR_H

#include <thread>
#include <atomic>
#include <functional>

class AdbMonitor {
public:
    AdbMonitor();
    ~AdbMonitor();

    void setOnDeviceConnected(std::function<void()> cb)    { on_connected_    = cb; }
    void setOnDeviceDisconnected(std::function<void()> cb) { on_disconnected_ = cb; }

    void start();
    void stop();

private:
    void pollLoop();
    bool hasConnectedDevice();
    void setupReversePorts();

    std::thread          thread_;
    std::atomic<bool>    running_{false};
    std::atomic<bool>    device_present_{false};
    std::function<void()> on_connected_;
    std::function<void()> on_disconnected_;
};

#endif
