#pragma once

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class EchoCommand {
public:
    EchoCommand(MqttHandler& mqtt){
        mqtt.registerCommand("echo", [](const JsonObject& request, JsonObject& response) {
            response["original"] = request;
        });
    }
};

}}}    // namespace farmhub::client::commands
