#pragma once

#include <ArduinoJson.h>
#include <functional>
#include <list>

#include <Farmhub.hpp>

using std::list;
using std::ref;
using std::reference_wrapper;

namespace farmhub { namespace client {

class BaseProperty {
public:
    BaseProperty(const char* name)
        : name(name) {
    }

    virtual void load(const JsonObject& json) = 0;
    virtual void store(JsonObject& json, bool maskSecrets) const = 0;

protected:
    const char* name;
};

class ConfigurationSerializer {
public:
    void add(BaseProperty& property) {
        auto reference = std::ref(property);
        properties.push_back(reference);
    }

    void load(const JsonObject& json) const {
        for (auto& property : properties) {
            property.get().load(json);
        }
    }

    void store(JsonObject& json) const {
        storeInternal(json, false);
    }

    void storeMaskingSecrets(JsonObject& json) const {
        storeInternal(json, true);
    }

private:
    void storeInternal(JsonObject& json, bool maskSecrets) const {
        for (auto& property : properties) {
            property.get().store(json, maskSecrets);
        }
    }

    list<reference_wrapper<BaseProperty>> properties;
};

template <typename T>
class Property : public BaseProperty {
public:
    Property(ConfigurationSerializer& serializer, const char* name, const T& value, const bool secret = false)
        : BaseProperty(name)
        , secret(secret)
        , value(value) {
        serializer.add(*this);
    }

    void set(const T& value) const {
        this->value = value;
    }

    const T& get() const {
        return value;
    }

    void load(const JsonObject& json) override {
        if (json.containsKey(name)) {
            value = json[name].as<T>();
        }
    }

    void store(JsonObject& json, bool maskSecrets) const override {
        if (maskSecrets && secret) {
            json[name] = "********";
        } else {
            json[name] = value;
        }
    }

private:
    const bool secret;
    T value;
};

class Configuration {
public:
    Configuration(const String& name, size_t capacity = 2048)
        : name(name)
        , capacity(capacity) {
    }

    virtual void update(const JsonObject& json) {
        load(json);
    }

protected:
    ConfigurationSerializer serializer;

    void load(const JsonObject& json) {
        serializer.load(json);

        // Print effective configuration
        DynamicJsonDocument prettyJson(2048);
        auto prettyRoot = prettyJson.to<JsonObject>();
        serializer.storeMaskingSecrets(prettyRoot);
        Serial.println("Effective " + name + " configuration:");
        serializeJsonPretty(prettyJson, Serial);
        Serial.println();

        onLoad(json);
    }

    virtual void onLoad(const JsonObject& json) {
        // Override if needed
    }

    const String name;
    const size_t capacity;
};

class FileConfiguration : public Configuration {
public:
    FileConfiguration(const String& name, const String& filename, size_t capacity = 2048)
        : Configuration(name, capacity)
        , filename(filename) {
    }

    void begin(bool reset = false) {
        DynamicJsonDocument json(capacity);
        if (reset) {
            Serial.println("Reset requested, falling back to default " + name + " configuration");
        } else if (!SPIFFS.exists(filename)) {
            Serial.println("The " + name + " configuration file " + filename + " was not found, falling back to defaults");
        } else {
            File file = SPIFFS.open(filename, FILE_READ);
            if (!file) {
                fatalError("Cannot open config file " + filename);
                return;
            }

            DeserializationError error = deserializeJson(json, file);
            file.close();
            if (error) {
                Serial.println(file.readString());
                fatalError("Failed to read config file " + filename + ": " + String(error.c_str()));
                return;
            }
        }
        load(json.as<JsonObject>());
    }

    void update(const JsonObject& json) override {
        Configuration::update(json);
        store();
    }

private:
    void store() const {
        File file = SPIFFS.open(filename, FILE_WRITE);
        if (!file) {
            fatalError("Cannot open config file " + filename);
            return;
        }

        DynamicJsonDocument json(capacity);
        auto root = json.as<JsonObject>();
        serializer.store(root);
        serializeJson(json, file);
    }

    const String filename;
};

}}    // namespace farmhub::client
