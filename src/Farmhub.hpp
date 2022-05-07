#pragma once

#include <Arduino.h>

// Arduino 2 constants for Arduino 1 compatibility
#ifndef ESP_ARDUINO_VERSION
#define ARDUINO_EVENT_WIFI_STA_GOT_IP SYSTEM_EVENT_STA_GOT_IP
#define ARDUINO_EVENT_WIFI_STA_LOST_IP SYSTEM_EVENT_STA_LOST_IP
#define ARDUINO_EVENT_WIFI_STA_CONNECTED SYSTEM_EVENT_STA_CONNECTED
#define ARDUINO_EVENT_WIFI_STA_DISCONNECTED SYSTEM_EVENT_STA_DISCONNECTED
#define GPIO_NUM_NC -1
#endif

namespace farmhub { namespace client {

void fatalError(String message) {
    Serial.println(message);
    Serial.flush();
    delay(10000);
    ESP.restart();
}

}}    // namespace farmhub::client
