#pragma once

#include <Events.hpp>
#include <Task.hpp>
#include <Telemetry.hpp>

#include "ValveScheduler.hpp"

using namespace std::chrono;
using namespace farmhub::client;

RTC_DATA_ATTR
int8_t valveHandlerStoredState;

enum class ValveState {
    CLOSED = -1,
    NONE = 0,
    OPEN = 1
};

class ValveController {
public:
    virtual void open() = 0;
    virtual void close() = 0;
    virtual void reset() = 0;
    virtual ValveState getDefaultState() = 0;
};

/**
 * @brief Handles the valve on an abstract level.
 *
 * Allows opening and closing via {@link ValveHandler#setState}.
 * Handles remote MQTT commands to open and close the valve.
 * Reports the valve's state via MQTT.
 */
class ValveHandler
    : public TelemetryProvider,
      public BaseTask {
public:
    ValveHandler(TaskContainer& tasks, MqttHandler& mqtt, EventHandler& events, ValveController& controller)
        : BaseTask(tasks, "ValveHandler")
        , events(events)
        , controller(controller) {
        mqtt.registerCommand("override", [&](const JsonObject& request, JsonObject& response) {
            ValveState targetState = request["state"].as<ValveState>();
            if (targetState == ValveState::NONE) {
                resume();
            } else {
                seconds duration = request.containsKey("duration")
                    ? request["duration"].as<seconds>()
                    : hours { 1 };
                override(targetState, duration);
                response["duration"] = duration;
            }
            response["state"] = state;
        });
    }

    void populateTelemetry(JsonObject& json) override {
        if (!enabled) {
            return;
        }
        json["valve"] = state;
        if (manualOverrideEnd != time_point<system_clock>()) {
            time_t rawtime = system_clock::to_time_t(manualOverrideEnd);
            auto timeinfo = gmtime(&rawtime);
            char buffer[80];
            strftime(buffer, 80, "%FT%TZ", timeinfo);
            json["overrideEnd"] = string(buffer);
        }
    }

    void begin() {
        controller.reset();

        // RTC memory is reset to 0 upon power-up
        if (valveHandlerStoredState == 0) {
            Serial.println("Initializing for the first time to default state");
            setState(controller.getDefaultState());
        } else {
            Serial.println("Initializing after waking from sleep with state = " + String(valveHandlerStoredState));
            state = valveHandlerStoredState == 1
                ? ValveState::OPEN
                : ValveState::CLOSED;
        }
        enabled = true;
    }

    void setSchedule(const JsonArray schedulesJson) {
        schedules.clear();
        if (schedulesJson.isNull() || schedulesJson.size() == 0) {
            Serial.println("No schedule defined");
        } else {
            Serial.println("Defining schedule:");
            for (JsonVariant scheduleJson : schedulesJson) {
                schedules.emplace_back(scheduleJson.as<JsonObject>());
                Serial.print(" - ");
                serializeJson(scheduleJson, Serial);
                Serial.println();
            }
        }
    }

    void override(ValveState state, seconds duration) {
        Serial.printf("Overriding valve to %d for %d seconds\n", static_cast<int>(state), duration.count());
        manualOverrideEnd = system_clock::now() + duration;
        setState(state);
    }

    void resume() {
        Serial.println("Normal valve operation resumed");
        manualOverrideEnd = time_point<system_clock>();
        auto defaultState = controller.getDefaultState();
    }

protected:
    const Schedule loop(const Timing& timing) override {
        if (!enabled) {
            return sleepIndefinitely();
        }

        auto now = system_clock::now();
        auto targetState = state;

        if (manualOverrideEnd != time_point<system_clock>()) {
            if (manualOverrideEnd >= now) {
                Serial.println("Manual override active");
                return sleepFor(seconds { 1 });
            }
            Serial.println("Manual override expired");
            resume();
            targetState = controller.getDefaultState();
        }

        if (!schedules.empty()) {
            targetState = scheduler.isScheduled(schedules, now)
                ? ValveState::OPEN
                : ValveState::CLOSED;
        }

        if (state != targetState) {
            Serial.println("Changing state");
            setState(targetState);
        }

        return sleepFor(seconds { 1 });
    }

private:
    void setState(ValveState state) {
        this->state = state;
        switch (state) {
            case ValveState::OPEN:
                Serial.println("Opening");
                valveHandlerStoredState = 1;
                controller.open();
                break;
            case ValveState::CLOSED:
                Serial.println("Closing");
                valveHandlerStoredState = -1;
                controller.close();
                break;
        }
        events.publishEvent("valve/state", [=](JsonObject& json) {
            json["state"] = state;
        });
    }

    ValveScheduler scheduler;
    EventHandler& events;
    ValveController& controller;

    ValveState state = ValveState::NONE;
    time_point<system_clock> manualOverrideEnd;
    bool enabled = false;
    std::list<ValveSchedule> schedules;
};

bool convertToJson(const ValveState& src, JsonVariant dst) {
    return dst.set(static_cast<int>(src));
}
void convertFromJson(JsonVariantConst src, ValveState& dst) {
    dst = static_cast<ValveState>(src.as<int>());
}
