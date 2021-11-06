#pragma once

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class EchoCommand {
public:
    EchoCommand(MqttHandler& mqtt)
        : mqtt(mqtt) {
        mqtt.registerCommand("echo", [&](const JsonObject& json) {
            Serial.println("Received echo command");
            serializeJsonPretty(json, Serial);
            DynamicJsonDocument response(json.size() + 128);
            response["echo"] = json;
            mqtt.publish("events/echo", response);
        });
    }

private:
    MqttHandler& mqtt;
};

}}}    // namespace farmhub::client::commands
