#include "MonitorControl.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>

#include "skreen_drm.h"

namespace {
constexpr const char* kSysfsDrmDir = "/sys/devices/platform/skreen/drm";
constexpr const char* kDefaultNode = "/dev/dri/card1";
constexpr const char* kSysfsModuleDir = "/sys/module/skreen_driver";
}  // namespace

std::string MonitorControl::findNode() {
    DIR* dir = opendir(kSysfsDrmDir);
    if (!dir)
        return {};

    std::string node;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        unsigned int idx;
        if (sscanf(entry->d_name, "card%u", &idx) == 1) {
            node = "/dev/dri/card" + std::to_string(idx);
            break;
        }
    }
    closedir(dir);
    return node;
}

MonitorControl::MonitorControl() {
    node_ = findNode();
    if (node_.empty()) {
        std::cerr << "[MonitorControl] Could not find the skreen device under "
                  << kSysfsDrmDir << ", falling back to " << kDefaultNode << "\n";
        node_ = kDefaultNode;
    }
}

bool MonitorControl::isAvailable() const {
    return access(node_.c_str(), R_OK | W_OK) == 0;
}

// The node is opened per call instead of being kept open. The first client to
// open a DRM primary node while it has no master becomes its DRM master, and
// a long-lived fd (also inherited by child processes such as the adb server
// without O_CLOEXEC) would keep that role, preventing mutter from taking
// master and driving the virtual monitor. Master is also dropped right away
// for the brief window the fd is open.
int MonitorControl::openNode() const {
    int fd = open(node_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        std::cerr << "[MonitorControl] open(" << node_ << "): " << strerror(errno) << "\n";
        return -1;
    }
    ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);
    return fd;
}

bool MonitorControl::setEnabled(bool enabled) {
    int fd = openNode();
    if (fd < 0)
        return false;

    drm_skreen_enable args{};
    args.enabled = enabled ? 1u : 0u;

    int ret = ioctl(fd, DRM_IOCTL_SKREEN_SET_ENABLED, &args);
    if (ret < 0)
        std::cerr << "[MonitorControl] DRM_IOCTL_SKREEN_SET_ENABLED: " << strerror(errno) << "\n";
    close(fd);
    return ret == 0;
}

bool MonitorControl::isEnabled(bool& enabled) const {
    int fd = openNode();
    if (fd < 0)
        return false;

    drm_skreen_enable args{};
    int ret = ioctl(fd, DRM_IOCTL_SKREEN_GET_ENABLED, &args);
    if (ret < 0)
        std::cerr << "[MonitorControl] DRM_IOCTL_SKREEN_GET_ENABLED: " << strerror(errno) << "\n";
    close(fd);
    if (ret < 0)
        return false;

    enabled = args.enabled != 0;
    return true;
}

MonitorControl::AddResolutionResult MonitorControl::addResolution(unsigned int width,
                                                                  unsigned int height) {
    int fd = openNode();
    if (fd < 0)
        return AddResolutionResult::Unavailable;

    drm_skreen_resolution args{};
    args.width = width;
    args.height = height;

    int ret = ioctl(fd, DRM_IOCTL_SKREEN_ADD_RESOLUTION, &args);
    int err = errno;
    close(fd);

    if (ret < 0) {
        switch (err) {
        case EEXIST: return AddResolutionResult::AlreadyListed;
        case EINVAL: return AddResolutionResult::OutOfRange;
        case ENOSPC: return AddResolutionResult::ListFull;
        default:
            std::cerr << "[MonitorControl] DRM_IOCTL_SKREEN_ADD_RESOLUTION: " << strerror(err) << "\n";
            return AddResolutionResult::Failed;
        }
    }
    return AddResolutionResult::Added;
}

bool MonitorControl::isDriverLoaded() {
    return access(kSysfsModuleDir, F_OK) == 0;
}

std::string MonitorControl::loadedDriverVersion() {
    std::ifstream file(std::string(kSysfsModuleDir) + "/version");
    std::string version;
    std::getline(file, version);
    return version;
}
