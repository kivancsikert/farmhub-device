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
    void begin() {
        init();
    }

    void loop() {
        tasks.loop();
    }

    class DeviceConfiguration : public FileConfiguration {
    public:
        DeviceConfiguration(
            const String& defaultType,
            const String& defaultModel,
            const String& defaultInstance = "default",
            const String& path = "/device-config.json",
            size_t capacity = 2048)
            : FileConfiguration("device", path, capacity)
            , type(serializer, "type", defaultType)
            , model(serializer, "model", defaultModel)
            , instance(serializer, "instance", defaultInstance)
            , description(serializer, "description", "")
            , mqttHost(serializer, "mqttHost", "")
            , mqttPort(serializer, "mqttPort", 1883)
            , mqttClientId(serializer, "mqttClientId", "")
            , mqttTopic(serializer, "mqttTopic", "") {
        }

        Property<String> type;
        Property<String> model;
        Property<String> instance;
        Property<String> description;

        Property<String> mqttHost;
        Property<unsigned int> mqttPort;
        Property<String> mqttClientId;
        Property<String> mqttTopic;

        virtual bool isResetButtonPressed() {
            return false;
        }

        virtual const String getHostname() {
            return type.get() + "-" + instance.get();
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
        , httpUpdateCommand(version)
        , tasks(maxSleepTime) {

        mqttHandler.registerCommand("echo", echoCommand);
        mqttHandler.registerCommand("restart", restartCommand);
        mqttHandler.registerCommand("files/list", fileListCommand);
        mqttHandler.registerCommand("files/read", fileReadCommand);
        mqttHandler.registerCommand("files/write", fileWriteCommand);
        mqttHandler.registerCommand("files/remove", fileRemoveCommand);
        mqttHandler.registerCommand("update", httpUpdateCommand);

        addTask(otaHandler);
        addTask(mqttHandler);
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
    void init() {
        Serial.begin(115200);
        Serial.printf("\nStarting %s version %s\n",
            name.c_str(), version.c_str());

        beginFileSystem();
        deviceConfig.begin();

        const String& hostname = deviceConfig.getHostname();

        Serial.printf("Running on %s %s instance '%s' with hostname '%s'\n",
            deviceConfig.type.get().c_str(), deviceConfig.model.get().c_str(),
            deviceConfig.instance.get().c_str(), hostname.c_str());

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
        mqttHandler.begin(
            deviceConfig.mqttHost.get(),
            deviceConfig.mqttPort.get(),
            deviceConfig.mqttClientId.get().isEmpty()
                ? deviceConfig.type.get() + "-" + deviceConfig.instance.get()
                : deviceConfig.mqttClientId.get(),
            deviceConfig.mqttTopic.get().isEmpty()
                ? "devices/" + deviceConfig.type.get() + "/" + deviceConfig.instance.get()
                : deviceConfig.mqttTopic.get());

        mqttHandler.publish(
            "init", [&](JsonObject& json) {
                json["type"] = deviceConfig.type.get();
                json["instance"] = deviceConfig.instance.get();
                json["model"] = deviceConfig.model.get();
                json["app"] = name;
                json["version"] = version;
            },
            true);

        beginApp();
    }

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

    DeviceConfiguration& deviceConfig;
    FileConfiguration& appConfig;
    WiFiProvider& wifiProvider;
    MdnsHandler mdnsHandler;
    MqttHandler mqttHandler;
    commands::EchoCommand echoCommand;
    commands::FileListCommand fileListCommand;
    commands::FileReadCommand fileReadCommand;
    commands::FileWriteCommand fileWriteCommand;
    commands::FileRemoveCommand fileRemoveCommand;
    commands::HttpUpdateCommand httpUpdateCommand;
    commands::RestartCommand restartCommand;
    OtaHandler otaHandler;
    TaskContainer tasks;
};

}}    // namespace farmhub::client
