#pragma once

#include <chrono>
#include <list>

#include <Farmhub.hpp>

using namespace std::chrono;

namespace farmhub { namespace client {

class SleepEvent {
private:
    SleepEvent(microseconds duration)
        : duration(duration) {
    }
    friend class SleepHandler;

public:
    const microseconds duration;
};

class WakeEvent {
private:
    WakeEvent(esp_sleep_source_t source)
        : source(source) {
    }
    friend class SleepHandler;

public:
    const esp_sleep_source_t source;
};

class SleepListener {
protected:
    virtual void onWake(WakeEvent& event) = 0;
    virtual void onDeepSleep(SleepEvent& event) = 0;
    friend class SleepHandler;
};

class SleepHandler {
public:
    SleepHandler() = default;

    void handleWake() {
        WakeEvent event { esp_sleep_get_wakeup_cause() };
        for (auto& listener : listeners) {
            listener.get().onWake(event);
        }
    }

    void deepSleepFor(microseconds duration) {
        Serial.printf("Sleeping for %ld seconds\n", (long) duration_cast<seconds>(duration).count());
        Serial.flush();

        SleepEvent event { duration };
        for (auto& listener : listeners) {
            listener.get().onDeepSleep(event);
        }

        esp_sleep_enable_timer_wakeup(duration.count());
        esp_deep_sleep_start();
    }

protected:
    void registerListener(SleepListener* listener) {
        listeners.emplace_back(*listener);
    }
    friend class BaseSleepListener;

private:
    std::list<std::reference_wrapper<SleepListener>> listeners;
};

class BaseSleepListener : public SleepListener {
protected:
    BaseSleepListener(SleepHandler& sleepHandler) {
        sleepHandler.registerListener(this);
    }

    virtual void onWake(WakeEvent& event) override {};
    virtual void onDeepSleep(SleepEvent& event) override {};
};

}}    // namespace farmhub::client
