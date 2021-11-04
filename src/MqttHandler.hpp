#pragma once

#include <ArduinoJson.h>
#include <ArduinoJsonConfig.hpp>
#include <CircularBuffer.h>
#include <Client.h>
#include <ESPmDNS.h>
#include <MQTT.h>
#include <WiFiClient.h>
#include <chrono>
#include <functional>
#include <list>

#include <Loopable.hpp>

#define MQTT_BUFFER_SIZE 2048
#define MQTT_QUEUED_MESSAGES_MAX 16

using namespace std::chrono;

namespace farmhub { namespace client {

class MqttHandler
    : public TimedLoopable<void> {
public:
    MqttHandler()
        : mqttClient(MQTT_BUFFER_SIZE)
        , host("host", "")
        , port("port", 1883)
        , clientId("clientId", "chicken-door")
        , prefix("prefix", "devices/chicken-door") {
        serializer.add(host);
        serializer.add(port);
        serializer.add(clientId);
        serializer.add(prefix);
    }

    void begin(
        const JsonObject& mqttConfig,
        std::function<void(const JsonObject&)> onConfigChange) {

        Serial.println("Initializing MQTT connector...");
        serializer.load(mqttConfig);

        Serial.printf("MQTT client ID is '%s', topic prefix is '%s'\n",
            clientId.get().c_str(), prefix.get().c_str());

        String configTopic = String(prefix.get()) + "/config";
        String commandTopicPrefix = String(prefix.get()) + "/commands/";

        mqttClient.setKeepAlive(180);
        mqttClient.setCleanSession(true);
        mqttClient.setTimeout(10000);
        mqttClient.onMessage([onConfigChange, configTopic, commandTopicPrefix, this](String& topic, String& payload) {
#ifdef DUMP_MQTT
            Serial.println("Received '" + topic + "' (size: " + payload.length() + "): " + payload);
#endif
            DynamicJsonDocument json(payload.length() * 2);
            deserializeJson(json, payload);
            if (topic == configTopic) {
                onConfigChange(json.as<JsonObject>());
            } else if (topic.startsWith(commandTopicPrefix)) {
                auto command = topic.substring(commandTopicPrefix.length());
                for (auto handler : commandHandlers) {
                    if (handler.command == command) {
                        handler.handle(json.as<JsonObject>());
                        return;
                    }
                }
                Serial.printf("Unknown command: '%s'\n", command.c_str());
            } else {
                Serial.printf("Unknown topic: '%s'\n", topic.c_str());
            }
        });
        mqttClient.begin(client);
    }

    bool publish(const String& topic, const JsonDocument& json, bool retain = false, int qos = 0) {
        String fullTopic = prefix.get() + "/" + topic;
#ifdef DUMP_MQTT
        Serial.printf("Queuing MQTT topic '%s'%s (qos = %d): ",
            fullTopic.c_str(), (retain ? " (retain)" : ""), qos);
        serializeJsonPretty(json, Serial);
        Serial.println();
#endif
        bool storedWithoutDropping = publishQueue.unshift(MqttMessage(fullTopic, json, retain, qos));
        if (!storedWithoutDropping) {
            Serial.println("Overflow in publish queue, dropping message");
        }
        return storedWithoutDropping;
    }

    bool subscribe(const String& topic, int qos) {
        if (!mqttClient.connected()) {
            return false;
        }
        String fullTopic = prefix.get() + "/" + topic;
        Serial.printf("Subscribing to MQTT topic '%s' with QOS = %d\n", fullTopic.c_str(), qos);
        bool success = mqttClient.subscribe(fullTopic.c_str(), qos);
        if (!success) {
            Serial.printf("Error subscribing to MQTT topic '%s', error = %d\n",
                fullTopic.c_str(), mqttClient.lastError());
        }
        return success;
    }

    void handleCommand(const String command, std::function<void(const JsonObject&)> handle) {
        commandHandlers.emplace_back(command, handle);
    }

protected:
    void timedLoop() override {
        if (!mqttClient.connected()) {
            if (!tryConnect()) {
                return;
            }
        }

        while (!publishQueue.isEmpty()) {
            MqttMessage message = publishQueue.pop();
            bool success = mqttClient.publish(message.topic, message.payload, message.retain, message.qos);
#ifdef DUMP_MQTT
            Serial.printf("Published to '%s' (size: %d)\n", message.topic.c_str(), message.payload.length());
#endif
            if (!success) {
                Serial.printf("Error publishing to MQTT topic at '%s', error = %d\n",
                    message.topic.c_str(), mqttClient.lastError());
            }
        }

        mqttClient.loop();
    }

    milliseconds nextLoopAfter() const override {
        return milliseconds { 500 };
    }

    void defaultValue() override {
    }

private:
    bool tryConnect() {
        // Lookup host name via MDNS explicitly
        // See https://github.com/kivancsikert/chicken-coop-door/issues/128
        String mdnsHost = host.get();
        int portNumber = port.get();
        IPAddress address;
        if (mdnsHost.isEmpty()) {
            auto count = MDNS.queryService("mqtt", "tcp");
            if (count > 0) {
                Serial.println("Found MQTT services via mDNS, choosing first:");
                for (int i = 0; i < count; i++) {
                    Serial.printf("  %d) %s:%d (%s)\n",
                        i + 1, MDNS.hostname(i).c_str(), MDNS.port(i), MDNS.IP(i).toString().c_str());
                }
                address = MDNS.IP(0);
                portNumber = (int) MDNS.port(0);
            } else {
                Serial.println("No MQTT services found via mDNS");
                return false;
            }
        }
        if (address == IPAddress()) {
            if (mdnsHost.endsWith(".local")) {
                mdnsHost = mdnsHost.substring(0, mdnsHost.length() - 6);
            }
            address = MDNS.queryHost(mdnsHost);
        }
        Serial.print("Connecting to MQTT broker at ");
        if (address == IPAddress()) {
            Serial.printf("%s:%d", host.get().c_str(), port.get());
            mqttClient.setHost(host.get().c_str(), portNumber);
        } else {
            Serial.printf("%s:%d", address.toString().c_str(), port.get());
            mqttClient.setHost(address, portNumber);
        }
        Serial.print("...");

        bool result = mqttClient.connect(clientId.get().c_str());

        if (!result) {
            Serial.printf(" failed, error = %d (check lwmqtt_err_t), return code = %d (check lwmqtt_return_code_t)\n",
                mqttClient.lastError(), mqttClient.returnCode());

            // Clean up the client
            mqttClient.disconnect();
            return false;
        }

        // We're now connected
        Serial.println(" connected");

        // Set QoS to 1 (ack) for configuration messages
        subscribe("config", 1);
        // QoS 0 (no ack) for commands
        subscribe("commands/+", 0);
        return true;
    }

    WiFiClient client;
    MQTTClient mqttClient;

    Property<String> host;
    Property<int> port;
    Property<String> clientId;
    Property<String> prefix;
    ConfigurationSerializer serializer;

    bool connecting = false;

    struct CommandHandler {
        CommandHandler(const String& command, std::function<void(const JsonObject&)> handle)
            : command(command)
            , handle(handle) {
        }

        const String command;
        const std::function<void(const JsonObject&)> handle;
    };

    std::list<CommandHandler> commandHandlers;

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

    CircularBuffer<MqttMessage, MQTT_QUEUED_MESSAGES_MAX> publishQueue;
};

}}    // namespace farmhub::client
