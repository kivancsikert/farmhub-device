#pragma once

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class RestartCommand {
public:
    RestartCommand(MqttHandler& mqtt) {
        mqtt.registerCommand("restart", [&](const JsonObject& request, JsonObject& response) {
            Serial.flush();
            ESP.restart();
        });
    }
};

}}}    // namespace farmhub::client::commands
