#include "MqttHandler.hpp"

namespace farmhub { namespace client {

MqttHandler::MqttHandler()
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

void MqttHandler::begin(
    const JsonObject& mqttConfig,
    std::function<void(const JsonObject&)> onConfigChange,
    std::function<void(const JsonObject&)> onCommand) {

    Serial.println("Initializing MQTT connector...");
    serializer.load(mqttConfig);

    Serial.printf("MQTT broker at '%s:%d', using client id '%s' (prefix: '%s')\n",
        host.get().c_str(), port.get(), clientId.get().c_str(), prefix.get().c_str());

    mqttClient.setKeepAlive(180);
    mqttClient.setCleanSession(true);
    mqttClient.setTimeout(10000);
    mqttClient.onMessage([onConfigChange, onCommand](String& topic, String& payload) {
#ifdef DUMP_MQTT
        Serial.println("Received '" + topic + "' (size: " + payload.length() + "): " + payload);
#endif
        DynamicJsonDocument json(payload.length() * 2);
        deserializeJson(json, payload);
        if (topic.endsWith("/config")) {
            onConfigChange(json.as<JsonObject>());
        } else if (topic.endsWith("/command")) {
            onCommand(json.as<JsonObject>());
        } else {
            Serial.printf("Unknown topic: '%s'\n", topic.c_str());
        }
    });
    mqttClient.begin(client);
}

void MqttHandler::timedLoop() {
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

bool MqttHandler::tryConnect() {
    Serial.printf("Connecting to MQTT at %s:%d", host.get().c_str(), port.get());

    // Lookup host name via MDNS explicitly
    // See https://github.com/kivancsikert/chicken-coop-door/issues/128
    String mdnsHost = host.get();
    int portNumber = port.get();
    IPAddress address;
    if (mdnsHost.isEmpty()) {
        auto count = MDNS.queryService("mqtt", "tcp");
        if (count > 0) {
            Serial.println("Found MQTT services via MDNS, choosing first:\n");
            for (int i = 0; i < count; i++) {
                Serial.printf("  - %s\n", MDNS.hostname(i).c_str());
            }
            address = MDNS.IP(0);
            portNumber = (int) MDNS.port(0);
        }
    }
    if (address == IPAddress()) {
        if (mdnsHost.endsWith(".local")) {
            mdnsHost = mdnsHost.substring(0, mdnsHost.length() - 6);
        }
        address = MDNS.queryHost(mdnsHost);
    }
    if (address == IPAddress()) {
        Serial.print(" using the host name");
        mqttClient.setHost(host.get().c_str(), portNumber);
    } else {
        Serial.print(" using IP " + address.toString());
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
    subscribe("command", 0);
    return true;
}

bool MqttHandler::publish(const String& topic, const JsonDocument& json, bool retain, int qos) {
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

bool MqttHandler::subscribe(const String& topic, int qos) {
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

}}    // namespace farmhub::client
