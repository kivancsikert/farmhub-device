#pragma once

#include <WiFi.h>
#include <WiFiManager.h>
#include <chrono>

#include <wifi/WiFiProvider.hpp>

using namespace std::chrono;

namespace farmhub { namespace client {

class AbstractWiFiManagerProvider
    : public WiFiProvider {
public:
    AbstractWiFiManagerProvider(seconds configurationTimeout)
        : configurationTimeout(configurationTimeout) {
    }

    virtual void begin(const String& hostname) override {
        // Explicitly set mode, ESP defaults to STA+AP
        WiFi.mode(WIFI_STA);

        // Reset settings - wipe stored credentials for testing;
        // these are stored by the ESP library
        // wm.resetSettings();

        // Must store hostname, because WiFiManager won't make a copy
        this->hostname = hostname;
        wm.setHostname(this->hostname.c_str());

        // Allow some time for connecting to the WIFI, otherwise
        // open configuration portal
        wm.setConnectTimeout(20);

        wm.setDebugOutput(false);

        wm.setWiFiAutoReconnect(true);
    }

protected:
    const seconds configurationTimeout;
    String hostname;
    WiFiManager wm;
};

class BlockingWiFiManagerProvider
    : public AbstractWiFiManagerProvider {
public:
    BlockingWiFiManagerProvider(seconds configurationTimeout = seconds { 0 })
        : AbstractWiFiManagerProvider(configurationTimeout) {
    }

    virtual void begin(const String& hostname) override {
        AbstractWiFiManagerProvider::begin(hostname);

        // Close the configuration portal after some time and reboot
        // if no WIFI is configured in that time
        wm.setConfigPortalTimeout(configurationTimeout.count());

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
    NonBlockingWiFiManagerProvider(TaskContainer& tasks, seconds configurationTimeout = seconds { 180 })
        : AbstractWiFiManagerProvider(configurationTimeout)
        , BaseTask(tasks, "WiFiManager") {};

    enum class State {
        UNINITIALIZED,
        INITIALIZED,
        CONFIGURING,
        CONNECTED
    };

    virtual void begin(const String& hostname) override {
        AbstractWiFiManagerProvider::begin(hostname);

        wm.setConfigPortalBlocking(false);
        state = State::INITIALIZED;
    }

protected:
    virtual const Schedule loop(const Timing& timing) override {
        if (state == State::UNINITIALIZED) {
            return sleepFor(seconds { 1 });
        }
        if (WiFi.isConnected()) {
            state = State::CONNECTED;
            // TODO Make this configurable
            return sleepAtMost(minutes { 1 });
        }

        if (state == State::CONFIGURING) {
            auto configurationTime = boot_clock::now() - configurationStartTime;
            if (configurationTime < configurationTimeout) {
                wm.process();
                return sleepFor(milliseconds { 100 });
            }
            Serial.println("WiFi: configuration timed out, trying to fall back to stored configuration");
            wm.stopConfigPortal();
            state = State::INITIALIZED;
        }

        // automatically connect using saved credentials if they exist
        // If connection fails it starts an access point with the specified name
        Serial.println("WiFi: trying to connect using stored credentials");
        String configHostname = hostname + "-config";
        if (wm.autoConnect(configHostname.c_str())) {
            Serial.println("WiFi: connected to WIFI");
            state = State::CONNECTED;
        } else {
            Serial.println("WiFi: configuration portal started, timeout: " + String((int) configurationTimeout.count()) + " seconds");
            state = State::CONFIGURING;
        }
        return sleepFor(milliseconds { 100 });
    }

private:
    State state = State::UNINITIALIZED;
    time_point<boot_clock> configurationStartTime;
};

}}    // namespace farmhub::client
