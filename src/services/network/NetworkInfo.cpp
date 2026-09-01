#include "NetworkInfo.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

std::vector<std::string> NetworkInfo::localIpv4Addresses() {
    std::vector<std::string> addresses;

    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0)
        return addresses;

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (ifa->ifa_flags & IFF_LOOPBACK)
            continue;
        if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_RUNNING))
            continue;

        char buf[INET_ADDRSTRLEN] = {0};
        auto* addr_in = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
        if (inet_ntop(AF_INET, &addr_in->sin_addr, buf, sizeof(buf)))
            addresses.emplace_back(buf);
    }

    freeifaddrs(ifaddr);
    return addresses;
}
