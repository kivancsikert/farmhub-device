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
        : Task("Uptime printer") {
    }

protected:
    const Schedule loop(time_point<steady_clock> scheduledTime) override {
        auto now = steady_clock::now();
        Serial.printf("Simple app has been running for %ld seconds (drift %ld ms)\n",
            (long) duration_cast<seconds>(now.time_since_epoch()).count(),
            (long) duration_cast<milliseconds>(now - scheduledTime).count());
        return repeatAsapAfter(seconds { 10 });
    }
};

class SimpleApp : public Application {
public:
    SimpleApp()
        : Application("SimpleApp", "UNKNOWN", wifiProvider)
        , telemetryPublisher(mqtt)
        , telemetryTask("Publish telemetry", seconds { 5 }, [&]() {
            telemetryPublisher.publish();
        }) {
        addTask(telemetryTask);
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
    IntervalTask telemetryTask;
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
