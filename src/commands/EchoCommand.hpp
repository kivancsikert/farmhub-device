#pragma once

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class EchoCommand {
public:
    EchoCommand(MqttHandler& mqtt){
        mqtt.registerCommand("echo", [](const JsonObject& command, MqttHandler::Responder& responder) {
            responder.respond([command](JsonObject& event) {
                event["original"] = command;
            });
        });
    }
};

}}}    // namespace farmhub::client::commands
