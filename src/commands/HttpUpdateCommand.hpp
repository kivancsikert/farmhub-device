#pragma once

#include <ArduinoJson.h>
#include <HTTPUpdate.h>

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class HttpUpdateCommand : public MqttHandler::Command {
public:
    HttpUpdateCommand(const String& currentVersion)
        : currentVersion(currentVersion) {
    }

    void handle(const JsonObject& request, JsonObject& response) override {
        if (!request.containsKey("url")) {
            Serial.println("Command contains no URL");
            return;
        }
        String url = request["url"];
        if (url.length() == 0) {
            Serial.println("Command contains empty url");
            return;
        }
        httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        update(url, currentVersion);
    }

private:
    static void update(const String& url, const String& currentVersion) {
        Serial.printf("Updating from version %s via URL %s\n", currentVersion.c_str(), url.c_str());
        WiFiClientSecure client;
        // Allow insecure connections for testing
        client.setInsecure();
        HTTPUpdateResult result = httpUpdate.update(client, url, currentVersion);
        switch (result) {
            case HTTP_UPDATE_FAILED:
                Serial.println(httpUpdate.getLastErrorString());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                Serial.println("No updates available");
                break;
            case HTTP_UPDATE_OK:
                Serial.println("Update OK");
                break;
            default:
                Serial.println("Unknown response");
                break;
        }
    }

    const String currentVersion;
};

}}}    // namespace farmhub::client::commands
