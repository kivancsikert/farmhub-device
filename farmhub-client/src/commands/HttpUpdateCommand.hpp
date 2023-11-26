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
            response["failure"] = "Command contains no URL";
            return;
        }
        String url = request["url"];
        if (url.length() == 0) {
            response["failure"] = "Command contains empty url";
            return;
        }
        httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        response["failure"] = update(url, currentVersion);
    }

private:
    static String update(const String& url, const String& currentVersion) {
        Serial.printf("Updating from version %s via URL %s\n", currentVersion.c_str(), url.c_str());
        WiFiClientSecure client;
        // Allow insecure connections for testing
        client.setInsecure();
        HTTPUpdateResult result = httpUpdate.update(client, url, currentVersion);
        switch (result) {
            case HTTP_UPDATE_FAILED:
                return httpUpdate.getLastErrorString() + " (" + String(httpUpdate.getLastError()) + ")";
            case HTTP_UPDATE_NO_UPDATES:
                return "No updates available";
            case HTTP_UPDATE_OK:
                return "Update OK";
            default:
                return "Unknown response";
        }
    }

    const String currentVersion;
};

}}}    // namespace farmhub::client::commands
