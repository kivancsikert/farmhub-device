#pragma once

#include <Buttons.hpp>

#include "../AbstractFlowControlApp.hpp"
#include "../Ds18B20SoilSensorHandler.hpp"
#include "BatteryHandler.hpp"
#include "Drv8874ValveController.hpp"
#include "ShtC3Handler.hpp"

using namespace farmhub::client;

class FlowControlDeviceConfig : public AbstractFlowControlDeviceConfig {
public:
    FlowControlDeviceConfig()
        : AbstractFlowControlDeviceConfig("mk5") {
    }

    gpio_num_t getLedPin() override {
        return GPIO_NUM_2;
    }

    bool getLedEnabledState() override {
        return LOW;
    }

    gpio_num_t getFlowMeterPin() override {
        return GPIO_NUM_5;
    }

    Drv8874ValveController::Config valve { this };
    Property<bool> builtInEnvironmentSensor { this, "builtInEnvironmentSensor", false };
};

class FlowControlApp : public AbstractFlowControlApp {
public:
    FlowControlApp()
        : AbstractFlowControlApp(deviceConfig, valveController) {
    }

    void beginPeripherials() override {
        resetWifi.begin(GPIO_NUM_0, INPUT_PULLUP);

        telemetryPublisher.registerProvider(battery);
        battery.begin(GPIO_NUM_1);
        if (deviceConfig.builtInEnvironmentSensor.get()) {
            telemetryPublisher.registerProvider(builtInEnvironment);
            builtInEnvironment.begin();
        } else {
            Serial.println("Built-in environment sensor is disabled");
        }

        soilSensor.begin(GPIO_NUM_7, GPIO_NUM_6);
        telemetryPublisher.registerProvider(soilSensor);

        valveController.begin(
            GPIO_NUM_16,    // IN1
            GPIO_NUM_17,    // IN2
            GPIO_NUM_11,    // Fault
            GPIO_NUM_10,    // Sleep
            GPIO_NUM_4      // Current
        );
    }

private:
    FlowControlDeviceConfig deviceConfig;
    BattertHandler battery;
    ShtC3Handler builtInEnvironment;
    Ds18B20SoilSensorHandler soilSensor;
    Drv8874ValveController valveController { deviceConfig.valve };
    HeldButtonListener resetWifi { tasks, "Reset WIFI", seconds { 5 },
        [&]() {
            Serial.println("Resetting WIFI settings");
            wifiProvider.resetSettings();

            // Blink the LED once for a second
            bool wasEnabled = led.isEnabled();
            led.setEnabled(!wasEnabled);
            delay(1000);
            led.setEnabled(wasEnabled);
        } };

    bool open = false;
};
