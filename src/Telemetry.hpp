#pragma once

#include "MqttHandler.hpp"

#include <ArduinoJson.h>
#include <functional>
#include <list>

namespace farmhub { namespace client {

class TelemetryProvider {
protected:
    virtual void populateTelemetry(JsonObject& json) = 0;
    friend class TelemetryPublisher;
};

class TelemetryPublisher {
public:
    TelemetryPublisher(
        MqttHandler& mqtt,
        const String& topic = "telemetry")
        : mqtt(mqtt)
        , topic(topic) {
    }

    void begin() {
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

private:
    MqttHandler& mqtt;
    const String topic;
    std::list<std::reference_wrapper<TelemetryProvider>> providers;
};

}}    // namespace farmhub::client
