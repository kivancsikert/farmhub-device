#pragma once

#include <Wire.h>

#include <SHTC3.h>

#include "../AbstractEnvironmentHandler.hpp"

using namespace farmhub::client;

class ShtC3Handler
    : public AbstractEnvironmentHandler {

public:
    ShtC3Handler() = default;

    void begin() {
        Serial.print("Initializing SHTC3 sensor\n");
        Wire.begin(GPIO_NUM_35, GPIO_NUM_36);

        if (sht.begin()) {
            enabled = true;
        } else {
            Serial.println("SHT.init() failed");
            enabled = false;
        }
    }

protected:
    void populateTelemetryInternal(JsonObject& json) override {
        if (!sht.sample()) {
            Serial.println("SHT.sample() failed");
            return;
        }
        auto temperature = sht.readTempC();
        auto humidity = sht.readHumidity();
        json["temperature"] = temperature;
        json["humidity"] = humidity;
    }

private:

    SHTC3 sht { Wire };
};
