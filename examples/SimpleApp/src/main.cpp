#include <Arduino.h>

#include <Application.hpp>
#include <Telemetry.hpp>
#include <wifi/WiFiManagerProvider.hpp>

using namespace farmhub::client;

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
        : Application("SimpleApp", "UNKNOWN", wifiProvider)
        , telemetryPublisher(mqtt) {
    }

protected:
    void beginApp() override {
        telemetryPublisher.registerProvider(telemetry);
    }

    void configurationUpdated(const JsonObject& json) override {
        Serial.println("Received MQTT config");
        serializeJsonPretty(json, Serial);
        Serial.println();
    }

    void loopApp() override {
        if (iterations++ % 50 == 0) {
            telemetryPublisher.publish();
        }

        delay(100);
    }

private:
    WiFiManagerProvider wifiProvider;
    SimpleTelemetryProvider telemetry;
    TelemetryPublisher telemetryPublisher;

    int iterations = 0;
};

SimpleApp app;

void setup() {
    app.begin("simple-app");
}

void loop() {
    app.loop();
}
