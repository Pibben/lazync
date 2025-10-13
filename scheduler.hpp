//
// Created by per on 2025-10-13.
//

#ifndef CATCH2TESTEXAMPLE_SCHEDULER_H
#define CATCH2TESTEXAMPLE_SCHEDULER_H
#include <condition_variable>
#include <coroutine>
#include <mutex>
#include <queue>

// Simple Scheduler for managing timed tasks
class Scheduler {
public:
    struct TimedTask {
        std::chrono::steady_clock::time_point wake_time;
        std::coroutine_handle<> coro;

        bool operator>(const TimedTask& other) const {
            return wake_time > other.wake_time;
        }
    };

    Scheduler() : running(true) {
        worker = std::thread([this]() { this->run(); });
    }

    ~Scheduler() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            running = false;
        }
        cv.notify_one();
        if (worker.joinable()) {
            worker.join();
        }
    }

    void schedule_after(std::coroutine_handle<> coro, std::chrono::milliseconds delay) {
        auto wake_time = std::chrono::steady_clock::now() + delay;
        {
            std::lock_guard<std::mutex> lock(mutex);
            tasks.push({wake_time, coro});
        }
        cv.notify_one();
    }

    void run() {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex);

            if (!tasks.empty()) {
                auto now = std::chrono::steady_clock::now();
                auto& top = tasks.top();

                if (top.wake_time <= now) {
                    auto coro = top.coro;
                    tasks.pop();
                    lock.unlock();
                    coro.resume();
                    continue;
                }

                // Wait until next task or notification
                cv.wait_until(lock, top.wake_time);
            } else {
                if (!running) break;
                cv.wait(lock);
            }
        }
    }

private:
    std::priority_queue<TimedTask, std::vector<TimedTask>, std::greater<TimedTask>> tasks;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> running;
};

// Global scheduler
inline Scheduler& get_scheduler() {
    static Scheduler scheduler;
    return scheduler;
}


#endif //CATCH2TESTEXAMPLE_SCHEDULER_H