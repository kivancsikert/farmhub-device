#pragma once

#include <functional>

#include <Application.hpp>
#include <Ntp.hpp>
#include <wifi/WiFiManagerProvider.hpp>

#include "MeterHandler.hpp"
#include "ValveHandler.hpp"
#include "version.h"

using namespace farmhub::client;

class AbstractFlowControlDeviceConfig : public Application::DeviceConfiguration {
public:
    AbstractFlowControlDeviceConfig(const String& defaultModel)
        : Application::DeviceConfiguration("flow-control", defaultModel) {
    }

    virtual gpio_num_t getLedPin() = 0;

    virtual bool getLedEnabledState() = 0;

    virtual gpio_num_t getFlowMeterPin() = 0;
};

class FlowControlAppConfig : public Application::AppConfiguration {
public:
    FlowControlAppConfig()
        : Application::AppConfiguration() {
    }

    MeterHandler::Config meter { this };
    Property<seconds> sleepPeriod { this, "sleepPeriod", seconds::zero() };
    RawJsonEntry schedule { this, "schedule" };
};

class LedHandler : public BaseSleepListener {
public:
    LedHandler(SleepHandler& sleep)
        : BaseSleepListener(sleep) {
    }

    void begin(gpio_num_t ledPin, bool enabledState) {
        this->ledPin = ledPin;
        this->enabledState = enabledState;
    }

    bool isEnabled() {
        return enabled;
    }

    void setEnabled(bool enabled) {
        this->enabled = enabled;
        digitalWrite(ledPin, enabled ^ !enabledState);
    }

protected:
    void onWake(WakeEvent& event) override {
        // Turn led on when we start
        Serial.printf("Woken by source: %d\n", event.source);
        pinMode(ledPin, OUTPUT);
        setEnabled(true);
    }

    void onDeepSleep(SleepEvent& event) override {
        // Turn off led when we go to sleep
        Serial.printf("Going to sleep, duration: %ld us\n", (long) event.duration.count());
        setEnabled(false);
    }

private:
    gpio_num_t ledPin;
    bool enabledState;
    bool enabled = false;
};

class AbstractFlowControlApp
    : public Application {
public:
    AbstractFlowControlApp(
        AbstractFlowControlDeviceConfig& deviceConfig, ValveController& valveController)
        : Application("ugly-duckling", VERSION, deviceConfig, config, wifiProvider)
        , deviceConfig(deviceConfig)
        , valve(tasks, mqtt, events, valveController) {
        telemetryPublisher.registerProvider(flowMeter);
        telemetryPublisher.registerProvider(valve);
        config.onUpdate([&]() {
            valve.setSchedule(config.schedule.get());
        });
    }

protected:
    void beginApp() override {
        ntp.begin();

        led.begin(deviceConfig.getLedPin(), deviceConfig.getLedEnabledState());
        flowMeter.begin(deviceConfig.getFlowMeterPin());

        beginPeripherials();

        valve.begin();
    }

    virtual void beginPeripherials() = 0;

private:
    void onSleep() {
        if (config.sleepPeriod.get() > seconds::zero()) {
            sleep.deepSleepFor(config.sleepPeriod.get());
        }
    }

    AbstractFlowControlDeviceConfig& deviceConfig;
    FlowControlAppConfig config;

    NtpHandler ntp { tasks, mdns };
    MeterHandler flowMeter { tasks, sleep, config.meter, std::bind(&AbstractFlowControlApp::onSleep, this) };

protected:
    NonBlockingWiFiManagerProvider wifiProvider { tasks };
    LedHandler led { sleep };
    ValveHandler valve;
};
