#pragma once

#include <Arduino.h>
#include <chrono>

using namespace std;

namespace farmhub { namespace client {

/**
  *  @brief Monotonic clock based on ESP's esp_timer_get_time()
  *
  *  Time returned has the property of only increasing at a uniform rate.
  */
struct boot_clock {
    typedef chrono::microseconds duration;
    typedef duration::rep rep;
    typedef duration::period period;
    typedef chrono::time_point<boot_clock, duration> time_point;

    static constexpr bool is_steady = true;

    static time_point now() noexcept {
        return time_point(duration(micros()));
    }
};

}}    // namespace farmhub::client
