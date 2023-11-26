#pragma once

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class EchoCommand : public MqttHandler::Command {
public:
    void handle(const JsonObject& request, JsonObject& response) override {
        response["original"] = request;
    }
};

}}}    // namespace farmhub::client::commands
