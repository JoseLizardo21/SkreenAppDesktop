#ifndef MONITOR_CONTROL_H
#define MONITOR_CONTROL_H

#include <string>

// Talks to the skreen kernel driver through its DRM ioctls to enable/disable
// the virtual monitor. See skreen_drive/IOCTL_USAGE.md for the protocol.
class MonitorControl {
public:
    enum class AddResolutionResult {
        Added,
        AlreadyListed,  // EEXIST: built-in preset or added before
        OutOfRange,     // EINVAL: outside the driver's XRES/YRES limits
        ListFull,       // ENOSPC: driver's extra-mode slots exhausted
        Unavailable,    // node could not be opened
        Failed,         // any other ioctl error
    };

    MonitorControl();

    MonitorControl(const MonitorControl&) = delete;
    MonitorControl& operator=(const MonitorControl&) = delete;

    // True if the skreen DRM node exists and can be opened read/write.
    bool isAvailable() const;

    bool setEnabled(bool enabled);

    // Returns false (and leaves 'enabled' untouched) on ioctl/availability
    // failure.
    bool isEnabled(bool& enabled) const;

    // Appends width x height to the connector's mode list. It does not change
    // the active resolution; the user picks it from the system display
    // settings. Added modes live in the driver's memory until it is reloaded.
    AddResolutionResult addResolution(unsigned int width, unsigned int height);

    // True if the skreen_driver kernel module is currently loaded.
    static bool isDriverLoaded();

    // MODULE_VERSION of the loaded module, or an empty string if it is not
    // loaded or was built before the driver exposed its version.
    static std::string loadedDriverVersion();

private:
    static std::string findNode();
    int openNode() const;

    std::string node_;
};

#endif
