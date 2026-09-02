#ifndef NETWORK_INFO_H
#define NETWORK_INFO_H

#include <string>
#include <vector>

class NetworkInfo {
public:
    // IPv4 de todas las interfaces activas y no-loopback (ej. wlan0, eth0).
    static std::vector<std::string> localIpv4Addresses();
};

#endif
