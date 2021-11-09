#pragma once

#include <ESPmDNS.h>
#include <functional>

#include <Farmhub.hpp>

class MdnsHandler {
public:
    MdnsHandler() = default;

    void begin(const String& hostname) {
        MDNS.begin(hostname.c_str());
    }

    bool withService(
        const String& serviceName,
        const String& port,
        const std::function<void(const String& hostname, const IPAddress& address, uint16_t port)> callback) {
        Serial.printf("Looking for %s/%s services via mDNS...",
            serviceName.c_str(), port.c_str());
        auto count = MDNS.queryService(serviceName.c_str(), port.c_str());
        if (count == 0) {
            return false;
        }
        Serial.printf(" found %d services, choosing first:\n", count);
        for (int i = 0; i < count; i++) {
            Serial.printf(" %s%d) %s:%d (%s)\n",
                i == 0 ? "*" : " ",
                i + 1,
                MDNS.hostname(i).c_str(),
                MDNS.port(i),
                MDNS.IP(i).toString().c_str());
        }
        callback(MDNS.hostname(0), MDNS.IP(0), MDNS.port(0));
        return true;
    }
};
