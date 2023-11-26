#pragma once

#include <cmath>

#include "../ValveHandler.hpp"

using namespace std::chrono;
using namespace farmhub::client;

enum class ValveControlStrategyType {
    NormallyOpen,
    NormallyClosed,
    Latching
};

class ValveControlStrategy {
public:
    virtual void open() = 0;
    virtual void close() = 0;
    virtual ValveState getDefaultState() = 0;

    virtual String describe() = 0;
};

class Drv8874ValveController
    : public ValveController {

private:
    const uint8_t PWM_IN1 = 0;                                    // PWM channel for IN1
    const uint8_t PWM_IN2 = 1;                                    // PWM channel for IN2
    const uint8_t PWM_RESOLUTION = 8;                             // 8 bit
    const int PWM_MAX_VALUE = std::pow(2, PWM_RESOLUTION) - 1;    // 2 ^ PWM_RESOLUTION - 1
    const uint32_t PWM_FREQ = 25000;                              // 25kHz

public:
    class Config
        : public NamedConfigurationSection {
    public:
        Config(ConfigurationSection* parent)
            : NamedConfigurationSection(parent, "valve") {
        }

        Property<ValveControlStrategyType> strategy { this, "strategy", ValveControlStrategyType::Latching };
        Property<milliseconds> switchDuration { this, "switchDuration", milliseconds { 500 } };
        Property<double> holdDuty { this, "holdDuty", 0.5 };
    };

    class HoldingValveControlStrategy
        : public ValveControlStrategy {

    public:
        HoldingValveControlStrategy(Drv8874ValveController& controller, milliseconds switchDuration, double holdDuty)
            : controller(controller)
            , switchDuration(switchDuration)
            , holdDuty(holdDuty) {
        }

    protected:
        void driveAndHold(bool phase) {
            controller.drive(phase, 1.0);
            delay(switchDuration.count());
            controller.drive(phase, holdDuty);
        }

        Drv8874ValveController& controller;
        const milliseconds switchDuration;
        const double holdDuty;
    };

    class NormallyClosedValveControlStrategy
        : public HoldingValveControlStrategy {
    public:
        NormallyClosedValveControlStrategy(Drv8874ValveController& controller, milliseconds switchDuration, double holdDuty)
            : HoldingValveControlStrategy(controller, switchDuration, holdDuty) {
        }

        void open() override {
            driveAndHold(HIGH);
        }

        void close() override {
            controller.stop();
        }

        ValveState getDefaultState() override {
            return ValveState::CLOSED;
        }

        String describe() override {
            return "normally closed with switch duration " + String((int) switchDuration.count()) + "ms and hold duty " + String(holdDuty * 100) + "%";
        }
    };

    class NormallyOpenValveControlStrategy
        : public HoldingValveControlStrategy {
    public:
        NormallyOpenValveControlStrategy(Drv8874ValveController& controller, milliseconds switchDuration, double holdDuty)
            : HoldingValveControlStrategy(controller, switchDuration, holdDuty) {
        }

        void open() override {
            controller.stop();
        }

        void close() override {
            driveAndHold(LOW);
        }

        ValveState getDefaultState() override {
            return ValveState::OPEN;
        }

        String describe() override {
            return "normally open with switch duration " + String((int) switchDuration.count()) + "ms and hold duty " + String(holdDuty * 100) + "%";
        }
    };

    class LatchingValveControlStrategy
        : public ValveControlStrategy {
    public:
        LatchingValveControlStrategy(Drv8874ValveController& controller, milliseconds switchDuration)
            : controller(controller)
            , switchDuration(switchDuration) {
        }

        void open() override {
            controller.drive(HIGH, 1.0);
            delay(switchDuration.count());
            controller.stop();
        }

        void close() override {
            controller.drive(LOW, 1.0);
            delay(switchDuration.count());
            controller.stop();
        }

        ValveState getDefaultState() override {
            return ValveState::NONE;
        }

        String describe() override {
            return "latching with switch duration " + String((int) switchDuration.count()) + "ms";
        }

    private:
        Drv8874ValveController& controller;
        const milliseconds switchDuration;
    };

    Drv8874ValveController(const Config& config)
        : config(config) {
    }

    // Note: on Ugly Duckling MK5, the DRV8874's PMODE is wired to 3.3V, so it's locked in PWM mode
    void begin(
        gpio_num_t in1Pin,
        gpio_num_t in2Pin,
        gpio_num_t faultPin,
        gpio_num_t sleepPin,
        gpio_num_t currentPin) {
        strategy = createStrategy(config);

        Serial.printf("Initializing DRV8874 valve handler on pins in1 = %d, in2 = %d, fault = %d, sleep = %d, current = %d, valve is %s\n",
            in1Pin, in2Pin, faultPin, sleepPin, currentPin, strategy->describe().c_str());

        this->in1Pin = in1Pin;
        this->in2Pin = in2Pin;
        this->faultPin = faultPin;
        this->sleepPin = sleepPin;
        this->currentPin = currentPin;

        pinMode(in1Pin, OUTPUT);
        pinMode(in2Pin, OUTPUT);
        pinMode(sleepPin, OUTPUT);
        ledcAttachPin(in1Pin, PWM_IN1);
        ledcSetup(PWM_IN1, PWM_FREQ, PWM_RESOLUTION);
        ledcAttachPin(in2Pin, PWM_IN2);
        ledcSetup(PWM_IN2, PWM_FREQ, PWM_RESOLUTION);

        pinMode(faultPin, INPUT);
        pinMode(currentPin, INPUT);

        digitalWrite(sleepPin, LOW);
    }

    void open() override {
        strategy->open();
    }

    void close() override {
        strategy->close();
    }

    ValveState getDefaultState() override {
        return strategy->getDefaultState();
    }

    void reset() override {
        stop();
    }

    void stop() {
        digitalWrite(sleepPin, LOW);
    }

    void drive(bool phase, double duty = 1) {
        digitalWrite(sleepPin, HIGH);

        int dutyValue = PWM_MAX_VALUE / 2 + (int) (PWM_MAX_VALUE / 2 * duty);
        Serial.printf("Driving valve %s at %f%%\n",
            phase ? "forward" : "reverse",
            duty * 100);
        ledcWrite(phase ? PWM_IN1 : PWM_IN2, dutyValue);
        ledcWrite(phase ? PWM_IN2 : PWM_IN1, 0);
    }

private:
    ValveControlStrategy* createStrategy(const Config& config) {
        switch (config.strategy.get()) {
            case ValveControlStrategyType::NormallyClosed:
                return new NormallyClosedValveControlStrategy(*this, config.switchDuration.get(), config.holdDuty.get());
            case ValveControlStrategyType::NormallyOpen:
                return new NormallyOpenValveControlStrategy(*this, config.switchDuration.get(), config.holdDuty.get());
            case ValveControlStrategyType::Latching:
                return new LatchingValveControlStrategy(*this, config.switchDuration.get());
            default:
                fatalError("Unknown strategy");
                throw "Unknown strategy";
        }
    }

    const Config& config;
    ValveControlStrategy* strategy;

    gpio_num_t in1Pin;
    gpio_num_t in2Pin;
    gpio_num_t faultPin;
    gpio_num_t sleepPin;
    gpio_num_t currentPin;
};

bool convertToJson(const ValveControlStrategyType& src, JsonVariant dst) {
    switch (src) {
        case ValveControlStrategyType::NormallyOpen:
            return dst.set("NO");
        case ValveControlStrategyType::NormallyClosed:
            return dst.set("NC");
        case ValveControlStrategyType::Latching:
            return dst.set("latching");
        default:
            Serial.println("Unknown strategy: " + String(static_cast<int>(src)));
            return dst.set("NC");
    }
}
void convertFromJson(JsonVariantConst src, ValveControlStrategyType& dst) {
    String strategy = src.as<String>();
    if (strategy == "NO") {
        dst = ValveControlStrategyType::NormallyOpen;
    } else if (strategy == "NC") {
        dst = ValveControlStrategyType::NormallyClosed;
    } else if (strategy == "latching") {
        dst = ValveControlStrategyType::Latching;
    } else {
        Serial.println("Unknown strategy: " + strategy);
        dst = ValveControlStrategyType::NormallyClosed;
    }
}
