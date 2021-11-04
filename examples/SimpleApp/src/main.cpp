#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include <ArduinoJsonConfig.hpp>
#include <HttpUpdateHandler.hpp>
#include <MqttHandler.hpp>
#include <OtaHandler.hpp>

using namespace farmhub::client;

const char* HOSTNAME = "simple-app";

OtaHandler ota;
MqttHandler mqtt;
HttpUpdateHandler httpUpdater;

void fatalError(String message) {
    Serial.println(message);
    Serial.flush();
    delay(10000);
    ESP.restart();
}

void setup() {
    Serial.begin(115200);
    Serial.println();

    // Explicitly set mode, ESP defaults to STA+AP
    WiFi.mode(WIFI_STA);

    WiFiManager wm;

    // Reset settings - wipe stored credentials for testing;
    // these are stored by the ESP library
    //wm.resetSettings();

    // Allow some time for connecting to the WIFI, otherwise
    // open configuration portal
    wm.setConnectTimeout(20);

    // Close the configuration portal after some time and reboot
    // if no WIFI is configured in that time
    wm.setConfigPortalTimeout(300);

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point
    // with an auto-generated SSID and no password,
    // then goes into a blocking loop awaiting
    // configuration and will return success result.
    if (!wm.autoConnect()) {
        fatalError("Failed to connect to WIFI");
    }

    if (!SPIFFS.begin()) {
        fatalError("Could not initialize file system");
    }

    WiFi.setHostname(HOSTNAME);
    MDNS.begin(HOSTNAME);
    ota.begin(HOSTNAME);

    File mqttConfigFile = SPIFFS.open("/mqtt-config.json", FILE_READ);
    DynamicJsonDocument mqttConfigJson(mqttConfigFile.size() * 2);
    DeserializationError error = deserializeJson(mqttConfigJson, mqttConfigFile);
    if (error) {
        Serial.println(mqttConfigFile.readString());
        fatalError("Failed to read MQTT config file at /mqtt-config.json: " + String(error.c_str()));
    }
    mqtt.begin(
        mqttConfigJson.as<JsonObject>(),
        [](const JsonObject& json) {
            Serial.println("Received MQTT config");
            serializeJsonPretty(json, Serial);
        });
    mqtt.handleCommand("echo", [](const JsonObject& json) {
        Serial.println("Received echo command");
        serializeJsonPretty(json, Serial);
    });
    httpUpdater.begin(mqtt);
}

int counter = 0;

void loop() {
    ota.loop();
    mqtt.loop();

    if (counter++ % 50 == 0) {
        Serial.print("Sending message... ");
        DynamicJsonDocument message(1024);
        message["message"] = "Hello World";
        message["counter"] = counter++;
        auto success = mqtt.publish("hello", message);
        Serial.println(success ? "OK" : "FAILED");
    }

    delay(100);
}
