#pragma once

#include <functional>

#include <Task.hpp>

using namespace farmhub::client;

class HeldButtonListener : public BaseTask {

public:
    HeldButtonListener(TaskContainer& tasks, const String& name, microseconds triggerDelay, std::function<void()> trigger)
        : BaseTask(tasks, name)
        , triggerDelay(triggerDelay)
        , trigger(trigger) {
    }

    void begin(gpio_num_t pin, uint8_t mode = INPUT_PULLUP) {
        this->pin = pin;
        Serial.printf("Initializing button \"%s\" on pin %d with mode = %d, trigger after %f sec\n",
            name.c_str(), pin, mode, triggerDelay.count() / 1000000.0);
        pinMode(pin, mode);
    }

protected:
    const Schedule loop(const Timing& timing) override {
        if (digitalRead(pin) == LOW) {
            if (!pressed) {
                pressed = true;
                pressedSince = timing.loopStartTime;
            } else if (!triggered && timing.loopStartTime - pressedSince > triggerDelay) {
                triggered = true;
                trigger();
            }
        } else {
            if (pressed) {
                pressed = false;
                triggered = false;
            }
        }
        return sleepFor(milliseconds { 100 });
    }

private:
    const microseconds triggerDelay;
    const std::function<void()> trigger;

    gpio_num_t pin;
    bool pressed = false;
    bool triggered = false;
    time_point<boot_clock> pressedSince;
};
