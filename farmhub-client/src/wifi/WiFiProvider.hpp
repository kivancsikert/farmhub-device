#pragma once

#include <Arduino.h>

namespace farmhub { namespace client {

class WiFiProvider {
public:
    virtual void begin(const String& hostname) = 0;

    /**
     * Reset stored configuration.
     */
    virtual void resetSettings() = 0;
};

}}    // namespace farmhub::client
