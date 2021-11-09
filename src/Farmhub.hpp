#pragma once

#include <Arduino.h>

namespace farmhub { namespace client {

void fatalError(String message) {
    Serial.println(message);
    Serial.flush();
    delay(10000);
    ESP.restart();
}

}}    // namespace farmhub::client
