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
    }

    const Schedule loop(const Timing& timing) override {
        switch (state) {
            case State::UP_TO_DATE:
                if (timing.loopStartTime - lastChecked > hours { 7 * 24 }) {
                    state = State::NEEDS_UPDATE;
                }
                break;
            case State::NEEDS_UPDATE: {
                if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("Not updating NTP because WIFI is not available");
                    return sleepFor(seconds { 10 });
                }
                Serial.println("Updating time from NTP");
                String ntpHost = fallbackServer;
                // mdns.withService("ntp", "udp",
                //     [&](const String& hostname, const IPAddress& address, uint16_t port) {
                //         ntpHost = address.toString();
                //     });
                Serial.println("Connecting to NTP at " + ntpHost);
                configTime(0, 0, ntpHost.c_str());
                Serial.printf("SNPT enabled = %d\n", sntp_enabled());
                updateStarted = timing.loopStartTime;
                state = State::UPDATING;
                return sleepFor(seconds { 1 });
            }
            case State::UPDATING:
                if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
                    long currentTime = time(nullptr);
                    Serial.printf("Current time is %ld\n", currentTime);
                    lastChecked = timing.loopStartTime;
                    state = State::UP_TO_DATE;
                    break;
                }
                if (timing.loopStartTime - updateStarted > seconds { 10 }) {
                    Serial.printf("NPT update timed out, state = %d\n", sntp_get_sync_status());
                    state = State::NEEDS_UPDATE;
                }
                return sleepFor(seconds { 1 });
        }
        // We are up-to-date, no need to check again anytime soon
        return sleepFor(hours { 1 });
    }

    bool isUpToDate() {
        return state == State::UP_TO_DATE;
    }

private:
    time_point<boot_clock> lastChecked;
    MdnsHandler& mdns;

    const String fallbackServer;

    enum class State {
        NEEDS_UPDATE,
        UPDATING,
        UP_TO_DATE
    };

    State state = State::NEEDS_UPDATE;
    time_point<boot_clock> updateStarted;
};

}}    // namespace farmhub::client
