#pragma once

#include "MqttHandler.hpp"

#include <ArduinoJson.h>
#include <functional>
#include <list>

namespace farmhub { namespace client {

class TelemetryProvider {
public:
    virtual void populateTelemetry(JsonObject& json) = 0;
};

class TelemetryPublisher {
public:
    TelemetryPublisher(
        MqttHandler& mqtt,
        const String& topic = "events")
        : mqtt(mqtt)
        , topic(topic) {
    }

    void begin() {
    }

    void registerProvider(TelemetryProvider& provider) {
        providers.push_back(std::reference_wrapper<TelemetryProvider>(provider));
    }

    void publish() {
        DynamicJsonDocument doc(2048);
        JsonObject root = doc.to<JsonObject>();
        root["uptime"] = millis();
        for (auto& provider : providers) {
            provider.get().populateTelemetry(root);
        }
        mqtt.publish(topic, doc);
    }

private:
    MqttHandler& mqtt;
    const String topic;
    std::list<std::reference_wrapper<TelemetryProvider>> providers;
};

}}    // namespace farmhub::client
