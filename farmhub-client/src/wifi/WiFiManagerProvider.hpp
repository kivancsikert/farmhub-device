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
    AbstractWiFiManagerProvider(seconds connectionTimeout, seconds configurationTimeout)
        : connectionTimeout(connectionTimeout)
        , configurationTimeout(configurationTimeout) {
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
        wm.setConnectTimeout(connectionTimeout.count());

        wm.setDebugOutput(false);

        wm.setWiFiAutoReconnect(true);
    }

    WiFiManager wm;

    void resetSettings() {
        wm.resetSettings();
    }

protected:
    const seconds connectionTimeout;
    const seconds configurationTimeout;
    String hostname;
};

class BlockingWiFiManagerProvider
    : public AbstractWiFiManagerProvider {
public:
    BlockingWiFiManagerProvider(
        seconds connectionTimeout = seconds { 20 },
        seconds configurationTimeout = seconds { 0 })
        : AbstractWiFiManagerProvider(connectionTimeout, configurationTimeout) {
    }

    virtual void begin(const String& hostname) override {
        AbstractWiFiManagerProvider::begin(hostname);

        // Close the configuration portal after some time and reboot
        // if no WIFI is configured in that time
        wm.setConfigPortalTimeout(configurationTimeout.count());

        // Automatically connect using saved credentials,
        // if connection fails, it starts an access point
        // with the host name for SSID and no password,
        // then goes into a blocking loop awaiting
        // configuration and will return success result.
        if (!wm.autoConnect(hostname.c_str())) {
            fatalError("Failed to connect to WIFI");
        }
    }
};

class NonBlockingWiFiManagerProvider
    : public AbstractWiFiManagerProvider,
      public BaseTask {
public:
    NonBlockingWiFiManagerProvider(
        TaskContainer& tasks,
        seconds connectionTimeout = seconds { 20 },
        seconds configurationTimeout = seconds { 180 },
        seconds connectionCheckInterval = minutes { 1 })
        : AbstractWiFiManagerProvider(connectionTimeout, configurationTimeout)
        , BaseTask(tasks, "WiFiManager")
        , connectionCheckInterval(connectionCheckInterval) {};

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
            return sleepAtMost(connectionCheckInterval);
        }

        auto now = boot_clock::now();
        if (state == State::CONFIGURING) {
            auto configurationTime = now - configurationStartTime;
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
        if (wm.autoConnect(hostname.c_str())) {
            Serial.println("WiFi: connected to WIFI");
            state = State::CONNECTED;
        } else {
            Serial.println("WiFi: configuration portal started, timeout: " + String((int) configurationTimeout.count()) + " seconds");
            configurationStartTime = now;
            state = State::CONFIGURING;
        }
        return sleepFor(milliseconds { 100 });
    }

private:
    const seconds connectionCheckInterval;
    State state = State::UNINITIALIZED;
    time_point<boot_clock> configurationStartTime;
};

}}    // namespace farmhub::client
