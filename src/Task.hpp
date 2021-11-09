#pragma once

#include <Arduino.h>
#include <chrono>
#include <functional>
#include <unordered_map>

using namespace std::chrono;
using std::reference_wrapper;
using std::unordered_map;

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
        Schedule(ScheduleType type, milliseconds period)
            : type(type)
            , period(period) {
        }

        const ScheduleType type;
        const milliseconds period;
    };

    /**
     * @brief Loops the task and returns how much time is expected to elapse before the next loop.
     *
     * Scheduling is best effort, which means that the task will not be called earlier than the
     * time returned by this function, but it may be called later.
     *
     * @param now the current time.
     * @return milliseconds the time until the next loop from <code>now</code>.
     */
    virtual const Schedule loop(time_point<system_clock> now) = 0;

    const String name;

protected:
    static const Schedule repeatImmediately() {
        return Schedule(ScheduleType::AFTER, milliseconds(0));
    }

    static const Schedule repeatAsapAfter(milliseconds period) {
        return Schedule(ScheduleType::AFTER, period);
    }

    static const Schedule repeatAlapBefore(milliseconds period) {
        return Schedule(ScheduleType::BEFORE, period);
    }
};

class TaskContainer {
public:
    TaskContainer() = default;

    void add(Task& task) {
        tasks.emplace_back(task);
    }

    void loop() {
        auto now = system_clock::now();
#ifdef LOG_TASKS
        Serial.printf("Now is %ld\n", (long) duration_cast<milliseconds>(now.time_since_epoch()).count());
#endif

        auto nextRound = now + seconds { 1 };
        for (auto& entry : tasks) {
#ifdef LOG_TASKS
            Serial.printf("Considering '%s' with next = %ld\n",
                entry.task.name.c_str(),
                (long) duration_cast<milliseconds>(entry.next.time_since_epoch()).count());
#endif
            if (now >= entry.next) {
#ifdef LOG_TASKS
                Serial.printf("Running '%s'...\n", entry.task.name.c_str());
#endif
                auto schedule = entry.task.loop(now);
                switch (schedule.type) {
                    case Task::ScheduleType::AFTER:
#ifdef LOG_TASKS
                        Serial.printf("Scheduling '%s' to execute ASAP after %ld\n",
                            entry.task.name.c_str(),
                            (long) duration_cast<milliseconds>(entry.next.time_since_epoch()).count());
#endif
                        entry.next = now + schedule.period;
                        break;
                    case Task::ScheduleType::BEFORE:
                        break;
                }
                nextRound = std::min(nextRound, now + schedule.period);
#ifdef LOG_TASKS
                Serial.printf("Next round will be at %ld\n",
                    (long) duration_cast<milliseconds>(nextRound.time_since_epoch()).count());
#endif
            } else {
#ifdef LOG_TASKS
                Serial.printf("Skipping '%s'...\n", entry.task.name.c_str());
#endif
            }
        }

        auto waitTime = duration_cast<milliseconds>(nextRound - system_clock::now());
        if (waitTime > milliseconds::zero()) {
#ifdef LOG_TASKS
            Serial.printf("Sleeping for %ld ms\n", (long) waitTime.count());
#endif
            delay(waitTime.count());
        }
    }

private:
    struct TaskEntry {
        TaskEntry(Task& task)
            : task(task)
            , next() {
        }

        Task& task;
        time_point<system_clock> next;
    };

    list<TaskEntry> tasks;
};

}}    // namespace farmhub::client
