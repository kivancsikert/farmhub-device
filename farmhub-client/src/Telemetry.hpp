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
        const String& topic = "telemetry",
        const MqttHandler::QoS qos = MqttHandler::QoS::AtLeastOnce)
        : IntervalTask(tasks, "Publish telemetry", interval, [&]() { publish(); })
        , mqtt(mqtt)
        , topic(topic)
        , qos(qos) {
    }

    template <typename Duration>
    TelemetryPublisher(
        TaskContainer& tasks,
        MqttHandler& mqtt,
        Property<Duration>& interval,
        const String& topic = "telemetry",
        const MqttHandler::QoS qos = MqttHandler::QoS::AtLeastOnce)
        : IntervalTask(tasks, "Publish telemetry", interval, [&]() { publish(); })
        , mqtt(mqtt)
        , topic(topic)
        , qos(qos) {
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
        mqtt.publish(topic, doc, MqttHandler::Retention::NoRetain, qos);
    }

private:
    MqttHandler& mqtt;
    const String topic;
    const MqttHandler::QoS qos;

    std::list<std::reference_wrapper<TelemetryProvider>> providers;
};

}}    // namespace farmhub::client
