#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_system.h>

#include <Configuration.hpp>
#include <Events.hpp>
#include <Farmhub.hpp>
#include <MdnsHandler.hpp>
#include <MqttHandler.hpp>
#include <OtaHandler.hpp>
#include <Sleep.hpp>
#include <Telemetry.hpp>
#include <commands/EchoCommand.hpp>
#include <commands/FileCommands.hpp>
#include <commands/HttpUpdateCommand.hpp>
#include <commands/PingCommand.hpp>
#include <commands/ResetWifiCommand.hpp>
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
            const String& path = "/device-config.json",
            size_t capacity = 2048)
            : FileConfiguration("device", path, capacity)
            , type(this, "type", defaultType)
            , model(this, "model", defaultModel)
            , instance(this, "instance", getMacAddress()) {
        }

        Property<String> type;
        Property<String> model;
        Property<String> instance;

        MqttHandler::Config mqtt { this, "mqtt" };

        virtual bool isResetButtonPressed() {
            return false;
        }

        virtual const String getHostname() {
            String hostname = instance.get();
            hostname.replace(":", "-");
            hostname.replace("?", "");
            return type.get() + "-" + hostname;
        }
    };

    class AppConfiguration : public FileConfiguration {
    public:
        AppConfiguration(seconds defaultHeartbeat = minutes { 1 })
            : FileConfiguration("application", "/config.json")
            , heartbeat(this, "heartbeat", defaultHeartbeat) {
        }

        Property<seconds> heartbeat;
    };

protected:
    Application(
        const String& name,
        const String& version,
        DeviceConfiguration& deviceConfig,
        AppConfiguration& appConfig,
        WiFiProvider& wifiProvider,
        microseconds maxSleepTime = minutes { 1 })
        : name(name)
        , version(version)
        , deviceConfig(deviceConfig)
        , appConfig(appConfig)
        , wifiProvider(wifiProvider)
        , resetWifiCommand(wifiProvider)
        , httpUpdateCommand(version)
        , tasks(maxSleepTime) {

        mqtt.registerCommand("echo", echoCommand);
        mqtt.registerCommand("ping", pingCommand);
        mqtt.registerCommand("reset-wifi", resetWifiCommand);
        mqtt.registerCommand("restart", restartCommand);
        mqtt.registerCommand("files/list", fileListCommand);
        mqtt.registerCommand("files/read", fileReadCommand);
        mqtt.registerCommand("files/write", fileWriteCommand);
        mqtt.registerCommand("files/remove", fileRemoveCommand);
        mqtt.registerCommand("update", httpUpdateCommand);
    }

    virtual void beginApp() {
    }

    const String name;
    const String version;

private:
    void init() {
        Serial.begin(115200);
        Serial.printf("\nStarting %s version %s\n",
            name.c_str(), version.c_str());

        beginFileSystem();

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

        mdns.begin(hostname, name, version);

        WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
            Serial.println("WiFi: connected to " + WiFi.SSID());
        },
            ARDUINO_EVENT_WIFI_STA_CONNECTED);
        WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
            Serial.printf("WiFi: got IP address: %s, hostname: '%s'\n",
                WiFi.localIP().toString().c_str(), WiFi.getHostname());
        },
            ARDUINO_EVENT_WIFI_STA_GOT_IP);
        WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
            Serial.println("WiFi: lost IP address");
        },
            ARDUINO_EVENT_WIFI_STA_LOST_IP);
        WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
            Serial.println("WiFi: disconnected");
        },
            ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        wifiProvider.begin(hostname);

        otaHandler.begin(hostname);

        deviceConfig.begin();
        String mqttClientId = deviceConfig.mqtt.clientId.get();
        if (mqttClientId.isEmpty()) {
            mqttClientId = name + "-" + deviceConfig.instance.get();
        }
        String mqttTopic = deviceConfig.mqtt.topic.get();
        if (mqttTopic.isEmpty()) {
            mqttTopic = "devices/" + name + "/" + deviceConfig.instance.get();
        }
        mqtt.begin(deviceConfig.mqtt.host.get(), deviceConfig.mqtt.port.get(), mqttClientId, mqttTopic);

        beginApp();

        sleep.handleWake();
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

    static const String& getMacAddress() {
        static String macAddress;
        if (macAddress.length() == 0) {
            uint8_t rawMac[8];
            for (int i = 0; i < 8; i++) {
                rawMac[i] = 0;
            }
            if (esp_efuse_mac_get_default(rawMac) != ESP_OK) {
                macAddress = "??:??:??:??:??:??:??:??";
            } else {
                char mac[24];
                sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                    rawMac[0], rawMac[1], rawMac[2], rawMac[3],
                    rawMac[4], rawMac[5], rawMac[6], rawMac[7]);
                macAddress = mac;
            }
        }
        return macAddress;
    }

    class ReportWakeUpHandler : BaseSleepListener {
    public:
        ReportWakeUpHandler(SleepHandler& sleep, MqttHandler& mqtt, const String& app, const String& version, const DeviceConfiguration& deviceConfig)
            : BaseSleepListener(sleep)
            , mqtt(mqtt)
            , app(app)
            , version(version)
            , deviceConfig(deviceConfig) {
        }

        void onWake(WakeEvent& event) override {
            mqtt.publish(
                "init",
                [&](JsonObject& json) {
                    json["type"] = deviceConfig.type.get();
                    json["model"] = deviceConfig.model.get();
                    json["instance"] = deviceConfig.instance.get();
                    json["mac"] = getMacAddress();
                    auto device = json.createNestedObject("deviceConfig");
                    deviceConfig.store(device, false);
                    json["app"] = app;
                    json["version"] = version;
                    json["wakeup"] = event.source;
                });
        }

    private:
        MqttHandler& mqtt;
        const String app;
        const String version;
        const DeviceConfiguration& deviceConfig;
    };

    DeviceConfiguration& deviceConfig;
    AppConfiguration& appConfig;
    WiFiProvider& wifiProvider;

public:
    TaskContainer tasks;
    MdnsHandler mdns;
    SleepHandler sleep;
    MqttHandler mqtt { tasks, mdns, sleep, appConfig };
    TelemetryPublisher telemetryPublisher { tasks, mqtt, appConfig.heartbeat };
    EventHandler events { mqtt, telemetryPublisher };

private:
    OtaHandler otaHandler { tasks };
    ReportWakeUpHandler wakeUpHandler { sleep, mqtt, name, version, deviceConfig };

    commands::EchoCommand echoCommand;
    commands::FileListCommand fileListCommand;
    commands::FileReadCommand fileReadCommand;
    commands::FileWriteCommand fileWriteCommand;
    commands::FileRemoveCommand fileRemoveCommand;
    commands::HttpUpdateCommand httpUpdateCommand;
    commands::ResetWifiCommand resetWifiCommand;
    commands::RestartCommand restartCommand;
    commands::PingCommand pingCommand { telemetryPublisher };
};

}}    // namespace farmhub::client
