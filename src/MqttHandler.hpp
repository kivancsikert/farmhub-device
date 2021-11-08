#pragma once

#include <ArduinoJson.h>
#include <CircularBuffer.h>
#include <Client.h>
#include <Configuration.hpp>
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
        : mqttClient(MQTT_BUFFER_SIZE) {
    }

    void begin(std::function<void(const JsonObject&)> onConfigChange) {
        Serial.println("Initializing MQTT connector...");
        config.begin();

        Serial.printf("MQTT client ID is '%s', topic prefix is '%s'\n",
            config.getClientId().c_str(), config.getTopic().c_str());

        String appConfigTopic = String(config.getTopic()) + "/config";
        String commandTopicPrefix = String(config.getTopic()) + "/commands/";

        mqttClient.setKeepAlive(180);
        mqttClient.setCleanSession(true);
        mqttClient.setTimeout(10000);
        mqttClient.onMessage([onConfigChange, appConfigTopic, commandTopicPrefix, this](String& topic, String& payload) {
#ifdef DUMP_MQTT
            Serial.println("Received '" + topic + "' (size: " + payload.length() + "): " + payload);
#endif
            DynamicJsonDocument json(payload.length() * 2);
            deserializeJson(json, payload);
            if (topic == appConfigTopic) {
                onConfigChange(json.as<JsonObject>());
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
        String fullTopic = config.getTopic() + "/" + topic;
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
        String fullTopic = config.getTopic() + "/" + topic;
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
        String mdnsHost = config.getHost();
        int portNumber = config.getPort();
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
            Serial.printf("%s:%d", config.getHost().c_str(), portNumber);
            mqttClient.setHost(config.getHost().c_str(), portNumber);
        } else {
            Serial.printf("%s:%d", address.toString().c_str(), portNumber);
            mqttClient.setHost(address, portNumber);
        }
        Serial.print("...");

        bool result = mqttClient.connect(config.getClientId().c_str());

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

    class MqttConfig : Configuration {
    public:
        MqttConfig()
            : Configuration()
            , host(serializer, "host", "")
            , port(serializer, "port", 1883)
            , clientId(serializer, "clientId", "")
            , topic(serializer, "prefix", "") {
        }

        void begin() {
            File mqttConfigFile = SPIFFS.open("/mqtt-config.json", FILE_READ);
            DynamicJsonDocument mqttConfigJson(mqttConfigFile.size() * 2);
            DeserializationError error = deserializeJson(mqttConfigJson, mqttConfigFile);
            mqttConfigFile.close();
            if (error) {
                Serial.println(mqttConfigFile.readString());
                fatalError("Failed to read MQTT config file at /mqtt-config.json: " + String(error.c_str()));
            }
            serializer.load(mqttConfigJson.as<JsonObject>());
        }

        const String& getHost() const {
            return host.get();
        }

        int getPort() const {
            return port.get();
        }

        const String& getClientId() const {
            return clientId.get();
        }

        const String& getTopic() const {
            return topic.get();
        }

    private:
        Property<String> host;
        Property<int> port;
        Property<String> clientId;
        Property<String> topic;
    };

    WiFiClient client;
    MQTTClient mqttClient;
    MqttConfig config;

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
