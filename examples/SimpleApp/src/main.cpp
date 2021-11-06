#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include <ArduinoJsonConfig.hpp>
#include <commands/HttpUpdateCommand.hpp>
#include <MqttHandler.hpp>
#include <OtaHandler.hpp>
#include <Telemetry.hpp>

using namespace farmhub::client;

const char* HOSTNAME = "simple-app";

class SimpleTelemetryProvider
    : public TelemetryProvider {
protected:
    void populateTelemetry(JsonObject& json) override {
        json["counter"] = counter++;
    }

private:
    unsigned int counter = 0;
};

OtaHandler ota;
MqttHandler mqtt;

SimpleTelemetryProvider telemetry;
TelemetryPublisher telemetryPublisher(mqtt);

void fatalError(String message) {
    Serial.println(message);
    Serial.flush();
    delay(10000);
    ESP.restart();
}

void beginFileSystem() {
    Serial.println("Starting up file system...");
    if (!SPIFFS.begin()) {
        fatalError("Could not initialize file system");
        return;
    }

    Serial.println("Contents:");
    File root = SPIFFS.open("/", FILE_READ);
    while (true) {
        File file = root.openNextFile();
        if (!file) {
            break;
        }
        Serial.print(" - ");
        Serial.println(file.name());
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println();

    beginFileSystem();

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
    mqtt.registerCommand("echo", [](const JsonObject& json) {
        Serial.println("Received echo command");
        serializeJsonPretty(json, Serial);
    });

    commands::HttpUpdateCommand::registerCommand(mqtt);

    telemetryPublisher.registerProvider(telemetry);
    telemetryPublisher.begin();
}

int iterations = 0;

void loop() {
    ota.loop();
    mqtt.loop();

    if (iterations++ % 50 == 0) {
        telemetryPublisher.publish();
    }

    delay(100);
}
