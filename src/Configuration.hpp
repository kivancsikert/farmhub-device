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
    Configuration() = default;

protected:
    ConfigurationSerializer serializer;
};

class FileConfiguration : public Configuration {
public:
    FileConfiguration(const String& filename)
        : filename(filename) {
    }

    void begin() {
        File file = SPIFFS.open(filename, FILE_READ);
        if (!file) {
            fatalError("Cannot open config file " + filename);
            return;
        }

        DynamicJsonDocument json(file.size() * 2);
        DeserializationError error = deserializeJson(json, file);
        file.close();
        if (error) {
            Serial.println(file.readString());
            fatalError("Failed to read config file " + filename + ": " + String(error.c_str()));
            return;
        }
        load(json.as<JsonObject>());
    }

    void update(const JsonObject& json) {
        load(json);
        store();
    }

    void store() const {
        File file = SPIFFS.open(filename, FILE_WRITE);
        if (!file) {
            fatalError("Cannot open config file " + filename);
            return;
        }

        DynamicJsonDocument json(2048);
        auto root = json.as<JsonObject>();
        serializer.store(root);
        serializeJson(json, file);
    }

protected:
    virtual void onLoad(const JsonObject& json) {
        // Override if needed
    }

private:
    void load(const JsonObject& json) {
        serializer.load(json);
        onLoad(json);
    }

    const String filename;
};

}}    // namespace farmhub::client
