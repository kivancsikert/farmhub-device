#pragma once

#include <Arduino.h>
#include <chrono>
#include <functional>
#include <list>

#include <BootClock.hpp>

using namespace std::chrono;

namespace farmhub { namespace client {

/**
 * @brief A repeating task with a name.
 */
class Task {
public:
    Task(const String& name)
        : name(name) {
    }

    enum class ScheduleType {
        /**
         * Schedule task to execute again after a given duration, as soon as possible.
         */
        AFTER,

        /**
         * Schedule task to execute again before a given duration, as late as possible.
         */
        BEFORE
    };

    struct Schedule {
        Schedule(ScheduleType type, microseconds delay)
            : type(type)
            , delay(delay) {
        }

        const ScheduleType type;
        const microseconds delay;
    };

    struct Timing {
        Timing(time_point<boot_clock> scheduledTime, time_point<boot_clock> loopStartTime)
            : scheduledTime(scheduledTime)
            , loopStartTime(loopStartTime) {
        }

        /**
         * @brief The time the task was scheduled to start.
         *
         * This might be earlier than the current time.
         */
        const time_point<boot_clock> scheduledTime;

        /**
         * @brief The time the current loop has started.
         *
         * This might be earlier than the current time.
         */
        const time_point<boot_clock> loopStartTime;
    };

    const String name;

protected:
    /**
     * @brief Loops the task and returns how much time is expected to elapse before the next loop.
     *
     * Scheduling is best effort, which means that the task will not be called earlier than the
     * time returned by this function, but it may be called later.
     *
     * @param timing the timing information of the current loop.
     * @return the schedule to keep based on <code>scheduledTime</code>.
     */
    virtual const Schedule loop(const Timing& timing) = 0;
    friend class TaskContainer;

    /**
     * @brief Repeat the task immediately after running other tasks.
     */
    static const Schedule yieldImmediately() {
        return Schedule(ScheduleType::AFTER, microseconds::zero());
    }

    /**
     * @brief Repeat the task as soon as possible after a given delay.
     *
     * It is guaranteed that the task will not repeat before the given delay.
     * The delay might be longer than specified if other tasks take longer to execute.
     */
    static const Schedule sleepFor(microseconds delay) {
        return Schedule(ScheduleType::AFTER, delay);
    }

    /**
     * @brief Repeat the task as late as possible before the given delay.
     *
     * We will execute the task again either after the given delay,
     * or when another task gets scheduled (whichever happens earlier).
     */
    static const Schedule sleepAtMost(microseconds delay) {
        return Schedule(ScheduleType::BEFORE, delay);
    }

    /**
     * @brief Repeat the task as late as possible.
     *
     * We will execute the task again when another task gets scheduled.
     */
    static const Schedule sleepIndefinitely() {
        return Schedule(ScheduleType::BEFORE, hours { 24 * 365 * 100 });
    }
};

class IntervalTask
    : public Task {
public:
    IntervalTask(const String& name, microseconds delay, std::function<void()> callback)
        : Task(name)
        , delay(delay)
        , callback(callback) {
    }

protected:
    const Schedule loop(const Timing& timing) override {
        callback();
        return sleepFor(delay);
    }

private:
    const microseconds delay;
    const std::function<void()> callback;
};

class TaskContainer {
public:
    TaskContainer(microseconds maxSleepTime)
        : maxSleepTime(maxSleepTime) {
    }

    void add(Task& task) {
        tasks.emplace_back(task);
    }

    void loop() {
        auto loopStartTime = boot_clock::now();
#ifdef LOG_TASKS
        Serial.printf("Loop starts at @%ld\n", (long) loopStartTime.time_since_epoch().count());
#endif

        auto nextRound = previousRound + maxSleepTime;
        for (auto& entry : tasks) {
#ifdef LOG_TASKS
            Serial.printf("Considering '%s' with next @%ld",
                entry.task.name.c_str(),
                (long) entry.next.time_since_epoch().count());
#endif
            if (loopStartTime >= entry.next) {
#ifdef LOG_TASKS
                Serial.print(", running...");
#endif
                auto scheduledTime = entry.next == time_point<boot_clock>()
                    ? loopStartTime
                    : entry.next;
                auto schedule = entry.task.loop(Task::Timing(scheduledTime, loopStartTime));
                auto nextScheduledTime = scheduledTime + schedule.delay;
                nextRound = std::min(nextRound, nextScheduledTime);
                switch (schedule.type) {
                    case Task::ScheduleType::AFTER:
#ifdef LOG_TASKS
                        Serial.printf(" Next execution scheduled ASAP after %ld us.\n",
                            (long) schedule.delay.count());
#endif
                        // Do not trigger before next scheduled time
                        entry.next = nextScheduledTime;
                        break;
                    case Task::ScheduleType::BEFORE:
#ifdef LOG_TASKS
                        Serial.printf(" Next execution scheduled ALAP before %ld us.\n",
                            (long) schedule.delay.count());
#endif
                        // Signal that once a ronud is triggered, we need to run regardless of when it happens
                        entry.next = time_point<boot_clock>();
                        break;
                }
            } else {
#ifdef LOG_TASKS
                Serial.println(", skipping.");
#endif
                nextRound = std::min(nextRound, entry.next);
            }
        }

        microseconds waitTime = nextRound - boot_clock::now();
        if (waitTime > microseconds::zero()) {
#ifdef LOG_TASKS
            Serial.printf("Sleeping for %ld us\n", (long) waitTime.count());
#endif
            delay(duration_cast<milliseconds>(waitTime).count());
        } else {
#ifdef LOG_TASKS
            Serial.println("Running next round immediately");
#endif
        }
        previousRound = nextRound;
    }

private:
    struct TaskEntry {
        TaskEntry(Task& task)
            : task(task)
            , next() {
        }

        Task& task;
        time_point<boot_clock> next;
    };

    const microseconds maxSleepTime;
    std::list<TaskEntry> tasks;
    time_point<boot_clock> previousRound;
};

}}    // namespace farmhub::client
