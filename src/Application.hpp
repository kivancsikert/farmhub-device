#pragma once

#include <Arduino.h>
#include <SPIFFS.h>

#include <Configuration.hpp>
#include <Farmhub.hpp>
#include <MqttHandler.hpp>
#include <OtaHandler.hpp>
#include <commands/EchoCommand.hpp>
#include <commands/FileCommands.hpp>
#include <commands/HttpUpdateCommand.hpp>
#include <commands/RestartCommand.hpp>
#include <wifi/WiFiProvider.hpp>

namespace farmhub { namespace client {

class Application {
public:
    void begin(const String& hostname) {
        Serial.begin(115200);
        Serial.printf("\nStarting %s version %s with hostname %s\n", name.c_str(), version.c_str(), hostname.c_str());

        beginFileSystem();
        wifiProvider.begin();
        WiFi.setHostname(hostname.c_str());
        MDNS.begin(hostname.c_str());
        ota.begin(hostname);
        mqtt.begin();

        beginApp();
    }

    void loop() {
        tasks.loop();
    }

protected:
    Application(const String& name, const String& version, WiFiProvider& wifiProvider)
        : name(name)
        , version(version)
        , wifiProvider(wifiProvider)
        , mqtt([&](const JsonObject& config) { configurationUpdated(config); })
        , echoCommand(mqtt)
        , fileCommands(mqtt)
        , httpUpdateCommand(mqtt, version)
        , restartCommand(mqtt) {

        addTask(ota);
        addTask(mqtt);
    }

    void addTask(Task& task) {
        tasks.add(task);
    }

    virtual void configurationUpdated(const JsonObject& config) {
    }

    const String name;
    const String version;
    WiFiProvider& wifiProvider;

    OtaHandler ota;
    MqttHandler mqtt;
    commands::EchoCommand echoCommand;
    commands::FileCommands fileCommands;
    commands::HttpUpdateCommand httpUpdateCommand;
    commands::RestartCommand restartCommand;

    virtual void beginApp() {
    }

private:
    TaskContainer tasks;

    void beginFileSystem() {
        Serial.print("Starting file system... ");
        if (!SPIFFS.begin()) {
            fatalError("Could not initialize file system");
            return;
        }

        Serial.println("contents:");
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
