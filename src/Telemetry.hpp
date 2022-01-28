#pragma once

#include <ArduinoJson.h>
#include <functional>
#include <list>

#include <MqttHandler.hpp>

namespace farmhub { namespace client {

class TelemetryProvider {
protected:
    virtual void populateTelemetry(JsonObject& json) = 0;
    friend class TelemetryPublisher;
};

class TelemetryPublisher
    : public IntervalTask {
public:
    TelemetryPublisher(
        TaskContainer& tasks,
        MqttHandler& mqtt,
        milliseconds interval,
        const String& topic = "telemetry")
        : IntervalTask(tasks, "Publish telemetry", interval, [&]() { publish(); })
        , mqtt(mqtt)
        , topic(topic) {
    }

    template <typename Duration>
    TelemetryPublisher(
        TaskContainer& tasks,
        MqttHandler& mqtt,
        Property<Duration>& interval,
        const String& topic = "telemetry")
        : IntervalTask(tasks, "Publish telemetry", interval, [&]() { publish(); })
        , mqtt(mqtt)
        , topic(topic) {
    }

    void registerProvider(TelemetryProvider& provider) {
        providers.push_back(std::reference_wrapper<TelemetryProvider>(provider));
    }

    void populate(JsonObject& json) {
        for (auto& provider : providers) {
            provider.get().populateTelemetry(json);
        }
    }

private:
    void publish() {
        DynamicJsonDocument doc(2048);
        JsonObject root = doc.to<JsonObject>();
        root["uptime"] = millis();
        populate(root);
        mqtt.publish(topic, doc);
    }

    MqttHandler& mqtt;
    const String topic;
    std::list<std::reference_wrapper<TelemetryProvider>> providers;
};

}}    // namespace farmhub::client
