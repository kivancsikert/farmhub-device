#pragma once

#include <Arduino.h>

#include "MqttHandler.hpp"
#include "Telemetry.hpp"

using namespace std::chrono;

namespace farmhub { namespace client {

class EventHandler {
public:
    EventHandler(MqttHandler& mqtt, TelemetryPublisher& telemetryPublisher)
        : mqtt(mqtt)
        , telemetryPublisher(telemetryPublisher) {
    }

    bool publishEvent(const String& event, std::function<void(JsonObject&)> populateEvent, bool skipTelemetry = false) {
        bool result = mqtt.publish(event, [populateEvent](JsonObject& json) {
            populateEvent(json);
        });
        if (!skipTelemetry) {
            telemetryPublisher.publish();
        }
        return result;
    }

private:
    MqttHandler& mqtt;
    TelemetryPublisher& telemetryPublisher;
};

}}    // namespace farmhub::client
