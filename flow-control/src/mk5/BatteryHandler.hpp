#pragma once

#include <Task.hpp>
#include <Telemetry.hpp>

using namespace farmhub::client;

class BattertHandler
    : public TelemetryProvider {
public:
    void begin(gpio_num_t batteryPin) {
        this->batteryPin = batteryPin;
        Serial.printf("Initializing battery handler on pin %d\n", batteryPin);

        pinMode(batteryPin, INPUT);
    }

protected:
    void populateTelemetry(JsonObject& json) override {
        // Volume is measured in liters
        auto batteryLevel = analogRead(batteryPin);
        json["battery"] = batteryLevel;
    }

private:
    gpio_num_t batteryPin;
};
