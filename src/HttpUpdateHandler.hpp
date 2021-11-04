#pragma once

#include <ArduinoJson.h>
#include <HTTPUpdate.h>

#include <MqttHandler.hpp>

namespace farmhub { namespace client {

class HttpUpdateHandler {
public:
    HttpUpdateHandler(MqttHandler& mqtt, const String& currentVersion = "UNKNOWN")
        : mqtt(mqtt)
        , currentVersion(currentVersion) {
        httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    }

    void begin() {
        mqtt.handleCommand("update", [this](const JsonObject& command) {
            if (!command.containsKey("url")) {
                Serial.println("Command contains no URL");
                return;
            }
            String url = command["url"];
            if (url.length() == 0) {
                Serial.println("Command contains empty url");
                return;
            }
            update(url);
        });
    }

private:
    void update(const String& url) {
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

    MqttHandler& mqtt;
    const String currentVersion;
};

}}    // namespace farmhub::client
