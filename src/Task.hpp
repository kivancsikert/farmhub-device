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

    /**
     * @brief Loops the task and returns how much time is expected to elapse before the next loop.
     *
     * Scheduling is best effort, which means that the task will not be called earlier than the
     * time returned by this function, but it may be called later.
     *
     * @param now the current time.
     * @return milliseconds the time until the next loop from <code>now</code>.
     */
    virtual milliseconds loop(time_point<system_clock> now) = 0;

    const String name;
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
                auto delay = entry.task.loop(now);
                entry.next = now + delay;
                nextRound = std::min(nextRound, entry.next);
#ifdef LOG_TASKS
                Serial.printf("Scheduling '%s' next at %ld\n",
                    entry.task.name.c_str(),
                    (long) duration_cast<milliseconds>(entry.next.time_since_epoch()).count());
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
