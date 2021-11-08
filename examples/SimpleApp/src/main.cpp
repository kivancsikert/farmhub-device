#include <Arduino.h>

#include <Application.hpp>
#include <Telemetry.hpp>
#include <WiFi.h>
#include <WiFiManager.h>

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

class SimpleApp : public Application {
public:
    SimpleApp()
        : Application("UNKNOWN")
        , telemetryPublisher(mqtt) {
    }

protected:
    void beginApp() override {
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
        mqttConfigFile.close();
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

        telemetryPublisher.registerProvider(telemetry);
    }

    void loopApp() override {
        if (iterations++ % 50 == 0) {
            telemetryPublisher.publish();
        }

        delay(100);
    }

private:
    SimpleTelemetryProvider telemetry;
    TelemetryPublisher telemetryPublisher;

    int iterations = 0;
};

SimpleApp app;

void setup() {
    app.begin();
}

void loop() {
    app.loop();
}
