#pragma once

#include <WiFi.h>
#include <esp_sntp.h>

#include "MdnsHandler.hpp"
#include "Task.hpp"

using namespace std::chrono;

namespace farmhub { namespace client {

void ntpUpdated(struct timeval* tv) {
    Serial.printf("NTP updated system clock to %ld\n", tv->tv_sec);
}

class NtpHandler
    : public BaseTask {
public:
    NtpHandler(TaskContainer& tasks, MdnsHandler& mdns, const String& fallbackServer = "time.google.com")
        : BaseTask(tasks, "NTP")
        , mdns(mdns)
        , fallbackServer(fallbackServer) {
    }

    void begin() {
        sntp_set_time_sync_notification_cb(ntpUpdated);
        begun = true;
    }

    const Schedule loop(const Timing& timing) override {
        if (!begun) {
            return sleepFor(seconds { 1 });
        }
        switch (state) {
            case State::CONNECTED:
                // Reconnect every week
                if (timing.loopStartTime - lastChecked > hours { 7 * 24 }) {
                    state = State::DISCONNECTED;
                }
                break;
            case State::DISCONNECTED: {
                if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("Not updating NTP because WIFI is not available");
                    return sleepFor(seconds { 10 });
                }
                Serial.println("Updating time from NTP");
                ntpHost = fallbackServer;
                mdns.withService(
                    "ntp", "udp",
                    [&](const String& hostname, const IPAddress& address, uint16_t port) {
                        ntpHost = address.toString();
                    });
                Serial.println("Connecting to NTP at " + ntpHost);
                configTime(0, 0, ntpHost.c_str());
                updateStarted = timing.loopStartTime;
                state = State::CONNECTING;
                break;
            }
            case State::CONNECTING:
                if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
                    long currentTime = time(nullptr);
                    Serial.printf("Current time is %ld\n", currentTime);
                    lastChecked = timing.loopStartTime;
                    state = State::CONNECTED;
                    return sleepFor(hours { 1 });
                }
                if (boot_clock::now() - updateStarted > seconds { 10 }) {
                    Serial.printf("NPT update timed out, state = %d\n", sntp_get_sync_status());
                    state = State::DISCONNECTED;
                    return sleepFor(seconds { 10 });
                }
                return sleepFor(seconds { 1 });
        }
        // We are up-to-date, no need to check again anytime soon
        return sleepFor(hours { 1 });
    }

    bool isUpToDate() {
        return state == State::CONNECTED;
    }

private:
    time_point<boot_clock> lastChecked;
    MdnsHandler& mdns;

    const String fallbackServer;
    // Need to retain server name because of https://github.com/espressif/arduino-esp32/issues/6720
    String ntpHost;

    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED
    };

    bool begun = false;
    State state = State::DISCONNECTED;
    time_point<boot_clock> updateStarted;
};

}}    // namespace farmhub::client
