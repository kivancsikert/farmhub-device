#include <Arduino.h>

#include <Application.hpp>
#include <Task.hpp>
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

class SimpleUptimeTask
    : public Task {
public:
    SimpleUptimeTask()
        : Task("Hello") {
    }

    milliseconds loop(time_point<system_clock> now) override {
        Serial.printf("Simple app has been running for %ld seconds\n",
            (long) duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
        return seconds { 1 };
    }
};

class SimpleApp : public Application {
public:
    SimpleApp()
        : Application("SimpleApp", "UNKNOWN", wifiProvider)
        , telemetryPublisher(mqtt, seconds { 5 }) {
        addTask(telemetryPublisher);
        addTask(uptimeTask);
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

private:
    WiFiManagerProvider wifiProvider;
    SimpleTelemetryProvider telemetry;
    TelemetryPublisher telemetryPublisher;
    SimpleUptimeTask uptimeTask;

    int iterations = 0;
};

SimpleApp app;

void setup() {
    app.begin("simple-app");
}

void loop() {
    app.loop();
}
