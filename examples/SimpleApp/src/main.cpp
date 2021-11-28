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

    Property<seconds> publishInterval { this, "publishInterval", seconds(5) };
    Property<seconds> uptimeInterval { this, "uptimeInterval", seconds(10) };
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
    : public BaseTask {
public:
    SimpleUptimeTask(TaskContainer& tasks, Property<seconds>& interval)
        : BaseTask(tasks, "Uptime printer")
        , interval(interval) {
    }

protected:
    const Schedule loop(const Timing& timing) override {
        auto now = boot_clock::now();
        microseconds drift = now - timing.scheduledTime;
        Serial.printf("Simple app has been running for %ld seconds (drift %ld us)\n",
            (long) duration_cast<seconds>(now.time_since_epoch()).count(),
            (long) drift.count());
        return sleepFor(interval.get());
    }

private:
    Property<seconds>& interval;
};

class SimpleApp : public Application {
public:
    SimpleApp()
        : Application("SimpleApp", "UNKNOWN", deviceConfig, appConfig, wifiProvider) {
    }

protected:
    void beginApp() override {
        telemetryPublisher.registerProvider(telemetry);
    }

private:
    SimpleDeviceConfig deviceConfig;
    SimpleAppConfig appConfig;
    NonBlockingWiFiManagerProvider wifiProvider { tasks() };
    SimpleTelemetryProvider telemetry;
    TelemetryPublisher telemetryPublisher { tasks(), appConfig.publishInterval, mqtt() };
    SimpleUptimeTask uptimeTask { tasks(), appConfig.uptimeInterval };

    int iterations = 0;
};

SimpleApp app;

void setup() {
    app.begin();
}

void loop() {
    app.loop();
}
