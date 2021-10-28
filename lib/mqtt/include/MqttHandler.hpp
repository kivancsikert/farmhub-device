#pragma once

#include <ArduinoJson.h>
#include <ArduinoJsonConfig.hpp>
#include <CircularBuffer.h>
#include <Client.h>
#include <ESPmDNS.h>
#include <MQTT.h>
#include <chrono>
#include <functional>

#define MQTT_BUFFER_SIZE 2048
#define MQTT_QUEUED_MESSAGES_MAX 16

using namespace std::chrono;

namespace devbase {

struct MqttMessage {
    MqttMessage()
        : topic("")
        , payload("")
        , retain(false)
        , qos(0) {
    }

    MqttMessage(const String& topic, const JsonDocument& payload, boolean retain, int qos)
        : topic(topic)
        , retain(retain)
        , qos(qos) {
        serializeJson(payload, this->payload);
    }

    String topic;
    String payload;
    boolean retain;
    int qos;
};

class MqttHandler {
public:
    MqttHandler();

    void begin(
        Client& client,
        const JsonObject& mqttConfig,
        std::function<void(const JsonObject&)> onConfigChange,
        std::function<void(const JsonObject&)> onCommand);

    bool publish(const String& topic, const JsonDocument& json, bool retained = false, int qos = 0);
    bool subscribe(const String& topic, int qos);

    void loop() {
        auto currentTime = system_clock::now();
        if (currentTime - previousLoop > milliseconds { 500 }) {
            previousLoop = currentTime;
            timedLoop();
        }
    }

private:
    bool tryConnect();
    void timedLoop();

    Client* client;
    MQTTClient mqttClient;

    Property<String> host;
    Property<int> port;
    Property<String> clientId;
    Property<String> prefix;
    ConfigurationSerializer serializer;

    bool connecting = false;

    CircularBuffer<MqttMessage, MQTT_QUEUED_MESSAGES_MAX> publishQueue;

    // See https://cloud.google.com/iot/docs/how-tos/exponential-backoff
    int __backoff__ = 1000;    // current backoff, milliseconds
    static const int __factor__ = 2.5f;
    static const int __minbackoff__ = 1000;      // minimum backoff, ms
    static const int __max_backoff__ = 60000;    // maximum backoff, ms
    static const int __jitter__ = 500;           // max random jitter, ms

    time_point<system_clock> previousLoop;
};

}
