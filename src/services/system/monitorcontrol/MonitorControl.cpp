#include "MonitorControl.h"

#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>

#include "skreen_drm.h"

namespace {
constexpr const char* kSysfsDrmDir = "/sys/devices/platform/skreen/drm";
constexpr const char* kDefaultNode = "/dev/dri/card1";
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

    fd_ = open(node_.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "[MonitorControl] open(" << node_ << "): " << strerror(errno) << "\n";
    }
}

MonitorControl::~MonitorControl() {
    if (fd_ >= 0)
        close(fd_);
}

bool MonitorControl::setEnabled(bool enabled) {
    if (fd_ < 0)
        return false;

    drm_skreen_enable args{};
    args.enabled = enabled ? 1u : 0u;

    if (ioctl(fd_, DRM_IOCTL_SKREEN_SET_ENABLED, &args) < 0) {
        std::cerr << "[MonitorControl] DRM_IOCTL_SKREEN_SET_ENABLED: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

bool MonitorControl::isEnabled(bool& enabled) const {
    if (fd_ < 0)
        return false;

    drm_skreen_enable args{};
    if (ioctl(fd_, DRM_IOCTL_SKREEN_GET_ENABLED, &args) < 0) {
        std::cerr << "[MonitorControl] DRM_IOCTL_SKREEN_GET_ENABLED: " << strerror(errno) << "\n";
        return false;
    }
    enabled = args.enabled != 0;
    return true;
}
