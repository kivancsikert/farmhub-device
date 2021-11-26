#include <Arduino.h>

#include <Application.hpp>
#include <Task.hpp>
#include <Telemetry.hpp>
#include <wifi/WiFiManagerProvider.hpp>

using namespace farmhub::client;

class SimpleDeviceConfig : public Application::DeviceConfiguration {
public:
    SimpleDeviceConfig()
        : Application::DeviceConfiguration("simple-example", "mk1") {
    }
};

class SimpleAppConfig : public FileConfiguration {
public:
    SimpleAppConfig()
        : FileConfiguration("application", "/config.json") {
    }
};

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
    const Schedule loop(const Timing& timing) override {
        auto now = boot_clock::now();
        microseconds drift = now - timing.scheduledTime;
        Serial.printf("Simple app has been running for %ld seconds (drift %ld us)\n",
            (long) duration_cast<seconds>(now.time_since_epoch()).count(),
            (long) drift.count());
        return sleepFor(seconds { 10 });
    }
};

class SimpleApp : public Application {
public:
    SimpleApp()
        : Application("SimpleApp", "UNKNOWN", deviceConfig, appConfig, wifiProvider)
        , telemetryPublisher(mqtt())
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

private:
    SimpleDeviceConfig deviceConfig;
    SimpleAppConfig appConfig;
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
