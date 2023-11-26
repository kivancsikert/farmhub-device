#pragma once

#include <ArduinoJson.h>
#include <SPIFFS.h>

#include <MqttHandler.hpp>

namespace farmhub { namespace client { namespace commands {

class FileListCommand : public MqttHandler::Command {
public:
    void handle(const JsonObject& request, JsonObject& response) override {
            File root = SPIFFS.open("/", FILE_READ);
            JsonArray files = response.createNestedArray("files");
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
    }
};

class FileReadCommand : public MqttHandler::Command {
public:
    void handle(const JsonObject& request, JsonObject& response) override {
        String path = request["path"];
        if (!path.startsWith("/")) {
            path = "/" + path;
        }
        Serial.printf("Reading %s\n", path.c_str());
        response["path"] = path;
        File file = SPIFFS.open(path, FILE_READ);
        if (file) {
            response["size"] = file.size();
            response["contents"] = file.readString();
        } else {
            response["error"] = "File not found";
        }
    }
};

class FileWriteCommand : public MqttHandler::Command {
public:
    void handle(const JsonObject& request, JsonObject& response) override {
            String path = request["path"];
            if (!path.startsWith("/")) {
                path = "/" + path;
            }
            Serial.printf("Writing %s\n", path.c_str());
            String contents = request["contents"];
            response["path"] = path;
            File file = SPIFFS.open(path, FILE_WRITE);
            if (file) {
                auto written = file.print(contents);
                file.flush();
                response["written"] = written;
                file.close();
            } else {
                response["error"] = "File not found";
            }
    }
};

class FileRemoveCommand : public MqttHandler::Command {
public:
    void handle(const JsonObject& request, JsonObject& response) override {
        String path = request["path"];
        if (!path.startsWith("/")) {
            path = "/" + path;
        }
        Serial.printf("Removing %s\n", path.c_str());
        response["path"] = path;
        if (SPIFFS.remove(path)) {
            response["removed"] = true;
        } else {
            response["error"] = "File not found";
        }
    }
};

}}}    // namespace farmhub::client::commands
