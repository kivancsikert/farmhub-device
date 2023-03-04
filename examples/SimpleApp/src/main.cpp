#include <Arduino.h>

#include <Application.hpp>
#include <Buttons.hpp>
#include <Ntp.hpp>
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

class SimpleAppConfig : public Application::AppConfiguration {
public:
    SimpleAppConfig()
        : Application::AppConfiguration(seconds { 5 }) {
    }

    Property<seconds> uptimeInterval { this, "uptimeInterval", seconds(10) };
    RawJsonEntry rawJson { this, "raw" };
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
        : Application("simple-example", "UNKNOWN", deviceConfig, appConfig, wifiProvider) {
        appConfig.onUpdate([this]() {
            Serial.println("Updated config!");
        });
    }

protected:
    void beginApp() override {
        telemetryPublisher.registerProvider(telemetry);
        ntp.begin();
        button.begin(GPIO_NUM_0);

        Serial.printf("Raw JSON in app config: (null: %s)", appConfig.rawJson.get().isNull() ? "true" : "false");
        serializeJson(appConfig.rawJson.get(), Serial);
        Serial.println();
    }

private:
    SimpleDeviceConfig deviceConfig;
    SimpleAppConfig appConfig;
    NonBlockingWiFiManagerProvider wifiProvider { tasks };
    NtpHandler ntp { tasks, mdns };
    SimpleTelemetryProvider telemetry;
    SimpleUptimeTask uptimeTask { tasks, appConfig.uptimeInterval };
    HeldButtonListener button { tasks, "Print message", seconds { 5 },
        []() {
            Serial.println("BOOT button held for 5 seconds");
        } };
};

SimpleApp app;

void setup() {
    app.begin();
}

void loop() {
    app.loop();
}
