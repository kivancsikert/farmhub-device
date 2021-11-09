#pragma once

#include <Arduino.h>

namespace farmhub { namespace client {

class WiFiProvider {
public:
    virtual void begin() = 0;
};

}}    // namespace farmhub::client
