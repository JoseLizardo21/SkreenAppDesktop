#ifndef MONITOR_CONTROL_H
#define MONITOR_CONTROL_H

#include <string>

// Talks to the skreen kernel driver through its DRM ioctls to enable/disable
// the virtual monitor. See skreen_drive/IOCTL_USAGE.md for the protocol.
class MonitorControl {
public:
    MonitorControl();
    ~MonitorControl();

    MonitorControl(const MonitorControl&) = delete;
    MonitorControl& operator=(const MonitorControl&) = delete;

    // True if the skreen DRM node was found and opened successfully.
    bool isAvailable() const { return fd_ >= 0; }

    bool setEnabled(bool enabled);

    // Returns false (and leaves 'enabled' untouched) on ioctl/availability
    // failure.
    bool isEnabled(bool& enabled) const;

private:
    static std::string findNode();

    std::string node_;
    int fd_ = -1;
};

#endif
