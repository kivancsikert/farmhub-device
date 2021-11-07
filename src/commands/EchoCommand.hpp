#pragma once

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class EchoCommand {
public:
    EchoCommand(MqttHandler& mqtt)
        : mqtt(mqtt) {
        mqtt.registerCommand("echo", [&](const JsonObject& command) {
            mqtt.publish("events/echo", [command](JsonObject& event) {
                event["original"] = command;
            });
        });
    }

private:
    MqttHandler& mqtt;
};

}}}    // namespace farmhub::client::commands
