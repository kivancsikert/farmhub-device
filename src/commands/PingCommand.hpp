#pragma once

#include <MqttHandler.hpp>
#include <Telemetry.hpp>

namespace farmhub { namespace client { namespace commands {

class PingCommand : public MqttHandler::Command {
public:
    PingCommand(TelemetryPublisher& telemetryPublisher)
        : telemetryPublisher(telemetryPublisher) {
    }

    void handle(const JsonObject& request, JsonObject& response) override {
        telemetryPublisher.publish();
        response["pong"] = millis();
    }

private:
    TelemetryPublisher& telemetryPublisher;
};

}}}    // namespace farmhub::client::commands
