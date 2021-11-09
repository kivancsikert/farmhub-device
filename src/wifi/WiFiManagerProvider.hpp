#pragma once

#include <WiFi.h>
#include <WiFiManager.h>

#include <wifi/WiFiProvider.hpp>

namespace farmhub { namespace client {

class WiFiManagerProvider
    : public WiFiProvider {
public:
    WiFiManagerProvider() = default;

    void begin() override {
        // Explicitly set mode, ESP defaults to STA+AP
        WiFi.mode(WIFI_STA);

        WiFiManager wm;

        // Reset settings - wipe stored credentials for testing;
        // these are stored by the ESP library
        //wm.resetSettings();

        // Allow some time for connecting to the WIFI, otherwise
        // open configuration portal
        wm.setConnectTimeout(20);

        // Close the configuration portal after some time and reboot
        // if no WIFI is configured in that time
        wm.setConfigPortalTimeout(300);

        // Automatically connect using saved credentials,
        // if connection fails, it starts an access point
        // with an auto-generated SSID and no password,
        // then goes into a blocking loop awaiting
        // configuration and will return success result.
        if (!wm.autoConnect()) {
            fatalError("Failed to connect to WIFI");
        }
    }
};

}}    // namespace farmhub::client
