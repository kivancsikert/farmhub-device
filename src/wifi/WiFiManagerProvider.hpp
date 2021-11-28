#pragma once

#include <WiFi.h>
#include <WiFiManager.h>

#include <wifi/WiFiProvider.hpp>

namespace farmhub { namespace client {

class AbstractWiFiManagerProvider
    : public WiFiProvider {
public:
    AbstractWiFiManagerProvider() = default;

    virtual void begin(const String& hostname) override {
        // Explicitly set mode, ESP defaults to STA+AP
        WiFi.mode(WIFI_STA);

        // Reset settings - wipe stored credentials for testing;
        // these are stored by the ESP library
        //wm.resetSettings();

        wm.setHostname(hostname.c_str());

        // Allow some time for connecting to the WIFI, otherwise
        // open configuration portal
        wm.setConnectTimeout(20);

        // Close the configuration portal after some time and reboot
        // if no WIFI is configured in that time
        wm.setConfigPortalTimeout(300);
    }

protected:
    WiFiManager wm;
};

class BlockingWiFiManagerProvider
    : public AbstractWiFiManagerProvider {
public:
    BlockingWiFiManagerProvider() = default;

    virtual void begin(const String& hostname) override {
        AbstractWiFiManagerProvider::begin(hostname);

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

class NonBlockingWiFiManagerProvider
    : public AbstractWiFiManagerProvider,
      public BaseTask {
public:
    NonBlockingWiFiManagerProvider(TaskContainer& tasks)
        : BaseTask(tasks, "WiFiManager") {
    };

    virtual void begin(const String& hostname) override {
        AbstractWiFiManagerProvider::begin(hostname);

        wm.setConfigPortalBlocking(false);

        //automatically connect using saved credentials if they exist
        //If connection fails it starts an access point with the specified name
        if (wm.autoConnect("AutoConnectAP")) {
            Serial.println("Connected to WIFI");
        } else {
            Serial.println("Configportal running");
        }
    }

protected:
    virtual const Schedule loop(const Timing& timing) override {
        return wm.process()
            ? sleepIndefinitely()
            : sleepFor(seconds(1));
    }
};

}}    // namespace farmhub::client
