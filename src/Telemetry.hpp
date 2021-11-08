#pragma once

#include <ArduinoJson.h>
#include <functional>
#include <list>

#include <MqttHandler.hpp>
#include <Task.hpp>

namespace farmhub { namespace client {

class TelemetryProvider {
protected:
    virtual void populateTelemetry(JsonObject& json) = 0;
    friend class TelemetryPublisher;
};

class TelemetryPublisher
    : public Task {
public:
    TelemetryPublisher(
        MqttHandler& mqtt,
        milliseconds interval,
        const String& topic = "telemetry")
        : Task("Telemetry publisher")
        , mqtt(mqtt)
        , interval(interval)
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

    void publish() {
        DynamicJsonDocument doc(2048);
        JsonObject root = doc.to<JsonObject>();
        root["uptime"] = millis();
        populate(root);
        mqtt.publish(topic, doc);
    }

protected:
    milliseconds loop(time_point<system_clock> now) override {
        publish();
        return interval;
    }

private:
    MqttHandler& mqtt;
    const milliseconds interval;
    const String topic;
    std::list<std::reference_wrapper<TelemetryProvider>> providers;
};

}}    // namespace farmhub::client
