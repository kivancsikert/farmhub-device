#pragma once

#include <Wire.h>

#include <SHT31.h>

#include "../AbstractEnvironmentHandler.hpp"

using namespace farmhub::client;

class Sht31Handler
    : public AbstractEnvironmentHandler {

    const int SHT31_ADDRESS = 0x44;

public:
    Sht31Handler() = default;

    void begin() {
        Serial.print("Initializing SHT31 sensor\n");
        Wire.begin();

        if (sht.begin()) {
            Wire.setClock(100000);
            // only supported by SHT3x
            sht.heatOff();
            enabled = true;
        } else {
            Serial.printf("SHT.init(): failed, error: %x\n", sht.getError());
            enabled = false;
        }
    }

protected:
    void populateTelemetryInternal(JsonObject& json) override {
        if (!sht.read()) {
            Serial.printf("SHT.read(): failed, error: %x\n", sht.getError());
            return;
        }
        auto temperature = sht.getTemperature();
        auto humidity = sht.getHumidity();
        json["temperature"] = temperature;
        json["humidity"] = humidity;
    }

private:

    SHT31 sht;
};
