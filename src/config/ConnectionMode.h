#ifndef CONNECTION_MODE_H
#define CONNECTION_MODE_H

#include <string>

enum class ConnectionMode { Cable, Wifi };

inline std::string connectionModeToString(ConnectionMode mode) {
    return mode == ConnectionMode::Wifi ? "wifi" : "cable";
}

inline ConnectionMode connectionModeFromString(const std::string& s) {
    return s == "wifi" ? ConnectionMode::Wifi : ConnectionMode::Cable;
}

#endif
