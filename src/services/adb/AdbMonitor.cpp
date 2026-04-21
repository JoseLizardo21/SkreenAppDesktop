#include "AdbMonitor.h"
#include <glib.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <chrono>
#include <thread>

struct IdleCallback {
    std::function<void()> fn;
};

static gboolean dispatch(gpointer data) {
    auto* cb = static_cast<IdleCallback*>(data);
    cb->fn();
    delete cb;
    return G_SOURCE_REMOVE;
}

AdbMonitor::AdbMonitor() = default;

AdbMonitor::~AdbMonitor() {
    stop();
}

void AdbMonitor::start() {
    running_ = true;
    thread_ = std::thread(&AdbMonitor::pollLoop, this);
}

void AdbMonitor::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

bool AdbMonitor::hasConnectedDevice() {
    FILE* pipe = popen("adb devices 2>/dev/null", "r");
    if (!pipe) return false;

    char buf[256];
    bool first_line = true;
    bool found = false;

    while (fgets(buf, sizeof(buf), pipe)) {
        if (first_line) { first_line = false; continue; }
        std::string line(buf);
        // A real device line looks like: "<serial>\tdevice"
        if (line.find("\tdevice") != std::string::npos) {
            found = true;
            break;
        }
    }
    pclose(pipe);
    return found;
}

void AdbMonitor::setupReversePorts() {
    std::system("adb reverse tcp:9002 tcp:9002 2>/dev/null");
    std::system("adb reverse tcp:9003 tcp:9003 2>/dev/null");
}

void AdbMonitor::pollLoop() {
    while (running_) {
        bool connected = hasConnectedDevice();

        if (connected && !device_present_) {
            device_present_ = true;
            setupReversePorts();
            if (on_connected_)
                g_idle_add(dispatch, new IdleCallback{on_connected_});

        } else if (!connected && device_present_) {
            device_present_ = false;
            if (on_disconnected_)
                g_idle_add(dispatch, new IdleCallback{on_disconnected_});
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
