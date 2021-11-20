#pragma once

#include <ArduinoJson.h>
#include <HTTPUpdate.h>

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class HttpUpdateCommand {
public:
    HttpUpdateCommand(MqttHandler& mqtt, const String& currentVersion = "UNKNOWN") {
        httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        mqtt.registerCommand("update", [currentVersion](const JsonObject& command, MqttHandler::Responder& responder) {
            if (!command.containsKey("url")) {
                Serial.println("Command contains no URL");
                return;
            }
            String url = command["url"];
            if (url.length() == 0) {
                Serial.println("Command contains empty url");
                return;
            }
            update(url, currentVersion);
        });
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
};

}}}    // namespace farmhub::client::commands
