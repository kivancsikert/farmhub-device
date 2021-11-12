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

    const String name;

protected:
    /**
     * @brief Loops the task and returns how much time is expected to elapse before the next loop.
     *
     * Scheduling is best effort, which means that the task will not be called earlier than the
     * time returned by this function, but it may be called later.
     *
     * @param scheduledTime the time the task was scheduled to execute. This might be earlier than
     *     the current time.
     * @return Schedule the schedule to keep based on <code>scheduledTime</code>.
     */
    virtual const Schedule loop(time_point<boot_clock> scheduledTime) = 0;
    friend class TaskContainer;

    static const Schedule repeatImmediately() {
        return Schedule(ScheduleType::AFTER, microseconds(0));
    }

    static const Schedule repeatAsapAfter(microseconds delay) {
        return Schedule(ScheduleType::AFTER, delay);
    }

    static const Schedule repeatAlapBefore(microseconds delay) {
        return Schedule(ScheduleType::BEFORE, delay);
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
    const Schedule loop(time_point<boot_clock> scheduledTime) override {
        callback();
        return repeatAsapAfter(delay);
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
        auto now = boot_clock::now();
#ifdef LOG_TASKS
        Serial.printf("Now @%ld\n", (long) now.time_since_epoch().count());
#endif

        auto nextRound = previousRound + maxSleepTime;
        for (auto& entry : tasks) {
#ifdef LOG_TASKS
            Serial.printf("Considering '%s' with next @%ld",
                entry.task.name.c_str(),
                (long) entry.next.time_since_epoch().count());
#endif
            if (now >= entry.next) {
#ifdef LOG_TASKS
                Serial.print(", running...");
#endif
                auto scheduledTime = entry.next == time_point<boot_clock>()
                    ? now
                    : entry.next;
                auto schedule = entry.task.loop(scheduledTime);
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
