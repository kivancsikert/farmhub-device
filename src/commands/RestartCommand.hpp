#pragma once

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class RestartCommand : public MqttHandler::Command {
public:
    void handle(const JsonObject& request, JsonObject& response) override {
        Serial.flush();
        ESP.restart();
    }
};

}}}    // namespace farmhub::client::commands
