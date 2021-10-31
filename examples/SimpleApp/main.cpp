#include <Arduino.h>
#include <WiFi.h>

#include <ArduinoJsonConfig.hpp>
#include <MqttHandler.hpp>

using namespace farmhub::client;

MqttHandler mqttHandler;

void setup() {
    DynamicJsonDocument mqttConfigJson(1024);
    mqttHandler.begin(
        mqttConfigJson.as<JsonObject>(),
        [](const JsonObject& json) {},
        [](const JsonObject& json) {});
}

void loop() {
}
