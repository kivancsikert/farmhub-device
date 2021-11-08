#pragma once

#include <Arduino.h>
#include <SPIFFS.h>

#include <ArduinoJsonConfig.hpp>
#include <MqttHandler.hpp>
#include <OtaHandler.hpp>
#include <commands/EchoCommand.hpp>
#include <commands/FileCommands.hpp>
#include <commands/HttpUpdateCommand.hpp>
#include <commands/RestartCommand.hpp>

namespace farmhub { namespace client {

class Application {
public:
    void begin() {
        Serial.begin(115200);
        Serial.println();

        beginFileSystem();

        beginApp();
    }

    void loop() {
        ota.loop();
        mqtt.loop();

        loopApp();
    }

protected:
    Application(const String& version)
        : version(version)
        , echoCommand(mqtt)
        , fileCommands(mqtt)
        , httpUpdateCommand(mqtt, version)
        , restartCommand(mqtt) {
    }

    void fatalError(String message) {
        Serial.println(message);
        Serial.flush();
        delay(10000);
        ESP.restart();
    }

    const String version;
    OtaHandler ota;
    MqttHandler mqtt;
    commands::EchoCommand echoCommand;
    commands::FileCommands fileCommands;
    commands::HttpUpdateCommand httpUpdateCommand;
    commands::RestartCommand restartCommand;

    virtual void beginApp() {
    }

    virtual void loopApp() {
    }

private:
    void beginFileSystem() {
        Serial.println("Starting up file system...");
        if (!SPIFFS.begin()) {
            fatalError("Could not initialize file system");
            return;
        }

        Serial.println("Contents:");
        File root = SPIFFS.open("/", FILE_READ);
        while (true) {
            File file = root.openNextFile();
            if (!file) {
                break;
            }
            Serial.printf(" - %s (%d bytes)\n", file.name(), file.size());
            file.close();
        }
    }
};

}}    // namespace farmhub::client
