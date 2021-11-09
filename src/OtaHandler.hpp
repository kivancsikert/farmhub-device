#pragma once

#include <Arduino.h>
#include <ArduinoOTA.h>

#include <Task.hpp>

namespace farmhub { namespace client {

class OtaHandler
    : public Task {

public:
    OtaHandler()
        : Task("OTA") {
    }

    void begin(const String& hostname) {
        ArduinoOTA.setHostname(hostname.c_str());

        ArduinoOTA.onStart([&]() {
            Serial.println("Start");
            updating = true;
        });
        ArduinoOTA.onEnd([&]() {
            Serial.println("\nEnd");
            updating = false;
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        });
        ArduinoOTA.onError([&](ota_error_t error) {
            Serial.printf("Web socket error[%u]\n", error);
            if (error == OTA_AUTH_ERROR) {
                Serial.println("Auth Failed");
            } else if (error == OTA_BEGIN_ERROR) {
                Serial.println("Begin Failed");
            } else if (error == OTA_CONNECT_ERROR) {
                Serial.println("Connect Failed");
            } else if (error == OTA_RECEIVE_ERROR) {
                Serial.println("Receive Failed");
            } else if (error == OTA_END_ERROR) {
                Serial.println("End Failed");
            } else {
                Serial.println("Other error");
            }
            updating = false;
        });
        ArduinoOTA.begin();
    }

    const Schedule loop(time_point<system_clock> now) override {
        ArduinoOTA.handle();
        return updating
            ? repeatImmediately()
            : repeatAsapAfter(seconds { 1 });
    }

private:
    bool updating = false;
};

}}    // namespace farmhub::client
