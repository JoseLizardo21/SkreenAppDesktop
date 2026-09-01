#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "StreamConfig.h"
#include "ConnectionMode.h"
#include <string>

class ConfigManager {
public:
    ConfigManager();
    StreamConfig load();
    void save(const StreamConfig& cfg);

    ConnectionMode loadConnectionMode();
    void saveConnectionMode(ConnectionMode mode);

private:
    std::string config_path_;
};

#endif
