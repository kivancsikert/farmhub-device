#pragma once

#include <Arduino.h>
#include <SPIFFS.h>

#include <Configuration.hpp>
#include <Farmhub.hpp>
#include <MdnsHandler.hpp>
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
        init(hostname);
    }

    void loop() {
        tasks.loop();
    }

    class DeviceConfiguration : public FileConfiguration {
    public:
        DeviceConfiguration(
            const String& defaultType,
            const String& defaultInstance = "default",
            const String& defaultModel = "mk1",
            const String& path = "/device-config.json",
            size_t capacity = 2048)
            : FileConfiguration("device", path, capacity)
            , type(serializer, "type", defaultType)
            , instance(serializer, "instance", defaultInstance)
            , model(serializer, "model", "mk1") {
        }

        Property<String> type;
        Property<String> instance;
        Property<String> model;

        virtual bool isResetButtonPressed() {
            return false;
        }
    };

protected:
    Application(
        const String& name,
        const String& version,
        DeviceConfiguration& deviceConfig,
        FileConfiguration& appConfig,
        WiFiProvider& wifiProvider,
        microseconds maxSleepTime = minutes { 1 })
        : name(name)
        , version(version)
        , deviceConfig(deviceConfig)
        , appConfig(appConfig)
        , wifiProvider(wifiProvider)
        , mqttHandler(mdnsHandler, appConfig)
        , echoCommand(mqttHandler)
        , fileCommands(mqttHandler)
        , httpUpdateCommand(mqttHandler, version)
        , restartCommand(mqttHandler)
        , tasks(maxSleepTime) {

        addTask(otaHandler);
        addTask(mqttHandler);
    }

    void init(const String& hostname) {
        Serial.begin(115200);
        Serial.printf("\nStarting %s version %s with hostname %s\n",
            name.c_str(), version.c_str(), hostname.c_str());

        beginFileSystem();
        deviceConfig.begin();
        Serial.printf("Running on %s %s instance '%s'\n",
            deviceConfig.type.get().c_str(), deviceConfig.model.get().c_str(), deviceConfig.instance.get().c_str());

        if (deviceConfig.isResetButtonPressed()) {
            Serial.println("Reset button pressed, skipping application configuration");
            appConfig.reset();
        } else {
            appConfig.begin();
        }

        wifiProvider.begin();
        WiFi.setHostname(hostname.c_str());
        mdnsHandler.begin(hostname, name, version);
        otaHandler.begin(hostname);
        mqttHandler.begin();

        mqttHandler.publish("init", [&](JsonObject& json) {
            json["type"] = deviceConfig.type.get();
            json["instance"] = deviceConfig.instance.get();
            json["model"] = deviceConfig.model.get();
            json["app"] = name;
            json["version"] = version;
        }, true);

        beginApp();
    }

    virtual void beginApp() {
    }

    void addTask(Task& task) {
        tasks.add(task);
    }

    MdnsHandler& mdns() {
        return mdnsHandler;
    }

    MqttHandler& mqtt() {
        return mqttHandler;
    }

    const String name;
    const String version;

private:
    DeviceConfiguration& deviceConfig;
    FileConfiguration& appConfig;
    WiFiProvider& wifiProvider;
    MdnsHandler mdnsHandler;
    MqttHandler mqttHandler;
    commands::EchoCommand echoCommand;
    commands::FileCommands fileCommands;
    commands::HttpUpdateCommand httpUpdateCommand;
    commands::RestartCommand restartCommand;
    OtaHandler otaHandler;

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

    TaskContainer tasks;
};

}}    // namespace farmhub::client
