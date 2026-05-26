#include "ConfigManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <iostream>

using json = nlohmann::json;
namespace fs = std::filesystem;

ConfigManager::ConfigManager() {
    const char* home = std::getenv("HOME");
    std::string base = home ? home : "/tmp";
    config_path_ = base + "/.config/skreenapp/config.json";
}

StreamConfig ConfigManager::load() {
    StreamConfig cfg;
    std::ifstream f(config_path_);
    if (!f.is_open()) return cfg;
    try {
        json j = json::parse(f);
        if (j.contains("stream")) {
            auto& s = j["stream"];
            if (s.contains("bitrate"))           cfg.bitrate           = s["bitrate"].get<int>();
            if (s.contains("keyframe_interval")) cfg.keyframe_interval = s["keyframe_interval"].get<int>();
            if (s.contains("encoder_speed"))     cfg.encoder_speed     = s["encoder_speed"].get<int>();
        }
    } catch (...) {
        std::cerr << "Config parse error, using defaults\n";
    }
    return cfg;
}

void ConfigManager::save(const StreamConfig& cfg) {
    fs::create_directories(fs::path(config_path_).parent_path());
    json j;
    j["stream"]["bitrate"]           = cfg.bitrate;
    j["stream"]["keyframe_interval"] = cfg.keyframe_interval;
    j["stream"]["encoder_speed"]     = cfg.encoder_speed;
    std::ofstream f(config_path_);
    if (f.is_open())
        f << j.dump(2) << "\n";
}
