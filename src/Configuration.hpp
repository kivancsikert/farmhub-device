#pragma once

#include <ArduinoJson.h>
#include <functional>
#include <list>

#include <Farmhub.hpp>

using std::list;
using std::ref;
using std::reference_wrapper;

namespace farmhub { namespace client {

class ConfigurationEntry {
public:
    virtual void load(const JsonObject& json) = 0;
    virtual void reset() = 0;
    virtual void store(JsonObject& json, bool maskSecrets) const = 0;
};

class ConfigurationSection : public ConfigurationEntry {
public:
    void add(ConfigurationEntry& entry) {
        auto reference = std::ref(entry);
        entries.push_back(reference);
    }

    virtual void load(const JsonObject& json) override {
        for (auto& entry : entries) {
            entry.get().load(json);
        }
    }

    virtual void reset() override {
        for (auto& entry : entries) {
            entry.get().reset();
        }
    }

    virtual void store(JsonObject& json, bool maskSecrets) const override {
        for (auto& entry : entries) {
            entry.get().store(json, maskSecrets);
        }
    }

private:
    list<reference_wrapper<ConfigurationEntry>> entries;
};

class NamedConfigurationSection : public ConfigurationSection {
public:
    NamedConfigurationSection(ConfigurationSection* parent, const String& name)
        : name(name) {
        parent->add(*this);
    }

    void load(const JsonObject& json) override {
        if (json.containsKey(name)) {
            ConfigurationSection::load(json[name]);
        } else {
            reset();
        }
    }

    void store(JsonObject& json, bool maskSecrets) const override {
        auto section = json.createNestedObject(name);
        ConfigurationSection::store(section, maskSecrets);
    }

private:
    const String name;
};

template <typename T>
class Property : public ConfigurationEntry {
public:
    Property(ConfigurationSection* parent, const String& name, const T& value, const bool secret = false)
        : name(name)
        , secret(secret)
        , value(value)
        , defaultValue(value) {
        parent->add(*this);
    }

    void set(const T& value) {
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

    void reset() override {
        value = defaultValue;
    }

    void store(JsonObject& json, bool maskSecrets) const override {
        if (maskSecrets && secret) {
            json[name] = "********";
        } else {
            json[name] = value;
        }
    }

private:
    const String name;
    const bool secret;
    T value;
    const T defaultValue;
};

class Configuration : protected ConfigurationSection {
public:
    Configuration(const String& name, size_t capacity = 2048)
        : name(name)
        , capacity(capacity) {
    }

    void reset() override {
        ConfigurationSection::reset();
    }

    virtual void update(const JsonObject& json) {
        load(json);
    }

protected:
    void load(const JsonObject& json) override {
        ConfigurationSection::load(json);

        // Print effective configuration
        DynamicJsonDocument prettyJson(2048);
        auto prettyRoot = prettyJson.to<JsonObject>();
        ConfigurationSection::store(prettyRoot, true);
        Serial.println("Effective " + name + " configuration:");
        serializeJsonPretty(prettyJson, Serial);
        Serial.println();

        onUpdate();
    }

    virtual void onUpdate() {
        // Override if needed
    }

    const String name;
    const size_t capacity;
};

class FileConfiguration : public Configuration {
public:
    FileConfiguration(const String& name, const String& path, size_t capacity = 2048)
        : Configuration(name, capacity)
        , path(path) {
    }

    void begin() {
        DynamicJsonDocument json(capacity);
        if (!SPIFFS.exists(path)) {
            Serial.println("The " + name + " configuration file " + path + " was not found, falling back to defaults");
        } else {
            File file = SPIFFS.open(path, FILE_READ);
            if (!file) {
                fatalError("Cannot open config file " + path);
                return;
            }

            DeserializationError error = deserializeJson(json, file);
            file.close();
            if (error) {
                Serial.println(file.readString());
                fatalError("Failed to read config file " + path + ": " + String(error.c_str()));
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
        File file = SPIFFS.open(path, FILE_WRITE);
        if (!file) {
            fatalError("Cannot open config file " + path);
            return;
        }

        DynamicJsonDocument json(capacity);
        auto root = json.to<JsonObject>();
        ConfigurationSection::store(root, false);
        serializeJson(json, file);
        file.close();
    }

    const String path;
};

}}    // namespace farmhub::client

namespace std { namespace chrono {

using namespace std::chrono;

template <typename Duration>
bool convertToJson(const Duration& src, JsonVariant dst) {
    return dst.set(src.count());
}

template <typename Duration>
void convertFromJson(JsonVariantConst src, Duration& dst) {
    dst = Duration { src.as<uint64_t>() };
}

}}    // namespace std::chrono
