#pragma once

#include <MqttHandler.hpp>
#include <wifi/WiFiProvider.hpp>

namespace farmhub { namespace client { namespace commands {

class ResetWifiCommand : public MqttHandler::Command {
public:
    ResetWifiCommand(WiFiProvider& wifiProvider)
        : wifiProvider(wifiProvider) {
    }

    void handle(const JsonObject& request, JsonObject& response) override {
        wifiProvider.resetSettings();
        response["reset"] = true;
    }
private:
    WiFiProvider& wifiProvider;
};

}}}    // namespace farmhub::client::commands
