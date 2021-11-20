#pragma once

#include <ArduinoJson.h>
#include <SPIFFS.h>

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class FileCommands {
public:
    FileCommands(MqttHandler& mqtt) {
        mqtt.registerCommand("files/list", [&](const JsonObject& command, MqttHandler::Responder& responder) {
            responder.respond([](JsonObject& event) {
                File root = SPIFFS.open("/", FILE_READ);
                JsonArray files = event.createNestedArray("files");
                while (true) {
                    File entry = root.openNextFile();
                    if (!entry) {
                        break;
                    }
                    JsonObject file = files.createNestedObject();
                    file["name"] = String(entry.name());
                    file["size"] = entry.size();
                    file["type"] = entry.isDirectory()
                        ? "dir"
                        : "file";
                    entry.close();
                }
            });
        });
        mqtt.registerCommand("files/read", [&](const JsonObject& command, MqttHandler::Responder& responder) {
            String path = command["path"];
            if (!path.startsWith("/")) {
                path = "/" + path;
            }
            Serial.printf("Reading %s\n", path.c_str());
            responder.respond([path](JsonObject& event) {
                event["path"] = path;
                File file = SPIFFS.open(path, FILE_READ);
                if (file) {
                    event["size"] = file.size();
                    event["contents"] = file.readString();
                } else {
                    event["error"] = "File not found";
                }
            });
        });
        mqtt.registerCommand("files/write", [&](const JsonObject& command, MqttHandler::Responder& responder) {
            String path = command["path"];
            if (!path.startsWith("/")) {
                path = "/" + path;
            }
            Serial.printf("Writing %s\n", path.c_str());
            String contents = command["contents"];
            responder.respond([path, contents](JsonObject& event) {
                event["path"] = path;
                File file = SPIFFS.open(path, FILE_WRITE);
                if (file) {
                    auto written = file.print(contents);
                    file.flush();
                    event["written"] = written;
                    file.close();
                } else {
                    event["error"] = "File not found";
                }
            });
        });
        mqtt.registerCommand("files/remove", [&](const JsonObject& command, MqttHandler::Responder& responder) {
            String path = command["path"];
            if (!path.startsWith("/")) {
                path = "/" + path;
            }
            Serial.printf("Removing %s\n", path.c_str());
            responder.respond([path](JsonObject& event) {
                event["path"] = path;
                if (SPIFFS.remove(path)) {
                    event["removed"] = true;
                } else {
                    event["error"] = "File not found";
                }
            });
        });
    }
};

}}}    // namespace farmhub::client::commands
