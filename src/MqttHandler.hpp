#pragma once

#include <ArduinoJson.h>
#include <CircularBuffer.h>
#include <Client.h>
#include <Configuration.hpp>
#include <ESPmDNS.h>
#include <MQTT.h>
#include <WiFi.h>
#include <chrono>
#include <functional>
#include <list>

#include <MdnsHandler.hpp>
#include <Task.hpp>

#define MQTT_BUFFER_SIZE 2048
#define MQTT_QUEUED_MESSAGES_MAX 16
#define MQTT_TIMEOUT 500
#define MQTT_POLL_FREQUENCY 1000

using namespace std::chrono;

namespace farmhub { namespace client {

class MqttHandler
    : public Task {
public:
    MqttHandler(MdnsHandler& mdns, Configuration& appConfig)
        : Task("MQTT")
        , mdns(mdns)
        , mqttClient(MQTT_BUFFER_SIZE)
        , appConfig(appConfig) {
    }

    void begin() {
        Serial.println("Initializing MQTT connector...");
        config.begin();

        Serial.printf("MQTT client ID is '%s', topic prefix is '%s'\n",
            config.clientId.get().c_str(), config.topic.get().c_str());

        String appConfigTopic = String(config.topic.get()) + "/config";
        String commandTopicPrefix = String(config.topic.get()) + "/commands/";

        mqttClient.setKeepAlive(180);
        mqttClient.setCleanSession(true);
        mqttClient.setTimeout(MQTT_TIMEOUT);
        mqttClient.onMessage([&, appConfigTopic, commandTopicPrefix](String& topic, String& payload) {
#ifdef DUMP_MQTT
            Serial.println("Received '" + topic + "' (size: " + payload.length() + "): " + payload);
#endif
            DynamicJsonDocument json(payload.length() * 2);
            deserializeJson(json, payload);
            if (topic == appConfigTopic) {
                appConfig.update(json.as<JsonObject>());
            } else if (topic.startsWith(commandTopicPrefix)) {
                auto command = topic.substring(commandTopicPrefix.length());
                Serial.printf("Received command '%s'\n", command.c_str());
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
        String fullTopic = config.topic.get() + "/" + topic;
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

    bool publish(const String& topic, std::function<void(JsonObject&)> populate, bool retain = false, int qos = 0, int size = MQTT_BUFFER_SIZE) {
        DynamicJsonDocument doc(size);
        JsonObject root = doc.to<JsonObject>();
        populate(root);
        return publish(topic, doc);
    }

    bool subscribe(const String& topic, int qos) {
        if (!mqttClient.connected()) {
            return false;
        }
        String fullTopic = config.topic.get() + "/" + topic;
        Serial.printf("Subscribing to MQTT topic '%s' with QOS = %d\n", fullTopic.c_str(), qos);
        bool success = mqttClient.subscribe(fullTopic.c_str(), qos);
        if (!success) {
            Serial.printf("Error subscribing to MQTT topic '%s', error = %d\n",
                fullTopic.c_str(), mqttClient.lastError());
        }
        return success;
    }

    void registerCommand(const String command, std::function<void(const JsonObject&)> handle) {
        commandHandlers.emplace_back(command, handle);
    }

protected:
    const Schedule loop(time_point<boot_clock> scheduledTime) override {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Waiting to connect to MQTT until WIFI is available");
            return repeatAsapAfter(seconds { 1 });
        }

        if (!mqttClient.connected()) {
            if (!tryConnect()) {
                // Try connecting again in 10 seconds
                return repeatAsapAfter(seconds { 10 });
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
        // TODO We could repeat sooner if we couldn't publish everything
        return repeatAsapAfter(milliseconds { MQTT_POLL_FREQUENCY });
    }

private:
    bool tryConnect() {
        // Lookup host name via MDNS explicitly
        // See https://github.com/kivancsikert/chicken-coop-door/issues/128
        String hostname = config.host.get();
        if (hostname.isEmpty()) {
            bool found = mdns.withService(
                "mqtt", "tcp",
                [&](const String& hostname, const IPAddress& address, uint16_t port) {
                    Serial.print("Connecting to MQTT broker at " + address.toString() + ":" + String(port));
                    mqttClient.setHost(address, port);
                });

            if (!found) {
                Serial.println("No MQTT services found via mDNS");
                return false;
            }
        } else {
            int port = config.port.get();
            bool found = mdns.withHost(hostname, [&, port](const IPAddress& address) {
                Serial.print("Connecting to MQTT broker at " + address.toString() + ":" + String(port));
                mqttClient.setHost(address, port);
            });
            if (!found) {
                return false;
            }
        }
        Serial.print("...");

        bool result = mqttClient.connect(config.clientId.get().c_str());

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
        subscribe("commands/#", 0);
        return true;
    }

    class MqttConfig : public FileConfiguration {
    public:
        MqttConfig()
            : FileConfiguration("MQTT", "/mqtt-config.json")
            , host(serializer, "host", "")
            , port(serializer, "port", 1883)
            , clientId(serializer, "clientId", "")
            , topic(serializer, "prefix", "") {
        }

        Property<String> host;
        Property<int> port;
        Property<String> clientId;
        Property<String> topic;
    };

    MdnsHandler mdns;
    WiFiClient client;
    MQTTClient mqttClient;
    MqttConfig config;
    Configuration& appConfig;

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
