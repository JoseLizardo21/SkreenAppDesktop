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

static json readExistingJson(const std::string& path) {
    json j;
    std::ifstream f(path);
    if (f.is_open()) {
        try { j = json::parse(f); } catch (...) {}
    }
    return j;
}

static void writeJson(const std::string& path, const json& j) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path);
    if (f.is_open())
        f << j.dump(2) << "\n";
}

StreamConfig ConfigManager::load() {
    StreamConfig cfg;
    json j = readExistingJson(config_path_);
    if (j.contains("stream")) {
        auto& s = j["stream"];
        if (s.contains("bitrate"))           cfg.bitrate           = s["bitrate"].get<int>();
        if (s.contains("keyframe_interval")) cfg.keyframe_interval = s["keyframe_interval"].get<int>();
        if (s.contains("encoder_speed"))     cfg.encoder_speed     = s["encoder_speed"].get<int>();
    }
    return cfg;
}

void ConfigManager::save(const StreamConfig& cfg) {
    // Lee el JSON existente primero para no pisar otras claves (ej. "connection").
    json j = readExistingJson(config_path_);
    j["stream"]["bitrate"]           = cfg.bitrate;
    j["stream"]["keyframe_interval"] = cfg.keyframe_interval;
    j["stream"]["encoder_speed"]     = cfg.encoder_speed;
    writeJson(config_path_, j);
}

ConnectionMode ConfigManager::loadConnectionMode() {
    json j = readExistingJson(config_path_);
    if (j.contains("connection") && j["connection"].contains("mode"))
        return connectionModeFromString(j["connection"]["mode"].get<std::string>());
    return ConnectionMode::Cable;
}

void ConfigManager::saveConnectionMode(ConnectionMode mode) {
    // Lee el JSON existente primero para no pisar otras claves (ej. "stream").
    json j = readExistingJson(config_path_);
    j["connection"]["mode"] = connectionModeToString(mode);
    writeJson(config_path_, j);
}
