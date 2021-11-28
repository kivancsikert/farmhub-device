#pragma once

#include <ESPmDNS.h>
#include <WiFi.h>
#include <functional>

#include <Farmhub.hpp>

class MdnsHandler {
public:
    MdnsHandler() = default;

    void begin(const String& hostname, const String& instanceName, const String& version) {
        WiFi.onEvent(
            [&](WiFiEvent_t event, WiFiEventInfo_t info) {
                MDNS.begin(hostname.c_str());
                MDNS.setInstanceName(instanceName);
                MDNS.addServiceTxt("arduino", "tcp", "version", version);
            },
            SYSTEM_EVENT_STA_GOT_IP);
    }

    // Lookup host name via MDNS explicitly
    // See https://github.com/kivancsikert/chicken-coop-door/issues/128
    bool withHost(const String& hostname, const std::function<void(const IPAddress&)> callback) {
        Serial.printf("Looking for host '%s' via mDNS... ", hostname.c_str());
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("cannot connect to mDNS because WiFi is not connected");
            return false;
        }
        String mdnsHost(hostname);
        if (mdnsHost.endsWith(".local")) {
            mdnsHost = mdnsHost.substring(0, mdnsHost.length() - 6);
        }
        IPAddress address = MDNS.queryHost(mdnsHost);
        if (address == IPAddress()) {
            WiFi.hostByName(mdnsHost.c_str(), address);
        }
        if (address == IPAddress()) {
            Serial.println("could not find host");
            return false;
        }
        Serial.println("found: " + address.toString());
        callback(address);
        return true;
    }

    bool withService(
        const String& serviceName,
        const String& port,
        const std::function<void(const String& hostname, const IPAddress& address, uint16_t port)> callback) {
        Serial.printf("Looking for %s/%s services via mDNS... ",
            serviceName.c_str(), port.c_str());
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("cannot connect to mDNS because WiFi is not connected");
            return false;
        }
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
