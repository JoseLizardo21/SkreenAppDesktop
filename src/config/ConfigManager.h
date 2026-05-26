#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "StreamConfig.h"
#include <string>

class ConfigManager {
public:
    ConfigManager();
    StreamConfig load();
    void save(const StreamConfig& cfg);

private:
    std::string config_path_;
};

#endif
