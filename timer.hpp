//
// Created by per on 2025-10-13.
//

#ifndef CATCH2TESTEXAMPLE_TIMER_HPP
#define CATCH2TESTEXAMPLE_TIMER_HPP

#include "scheduler.hpp"
#include "task.hpp"

#include <chrono>

// Sleep awaitable that uses the scheduler
struct SleepAwaitable {
    SleepAwaitable(std::chrono::milliseconds duration) : duration(duration) {}
    std::chrono::milliseconds duration;
    Scheduler::TimerHandle timerHandle;

    bool await_ready() { return duration.count() == 0; }

    void await_suspend(std::coroutine_handle<> coro) {
        get_scheduler().schedule_after(coro, duration, timerHandle);
    }

    void await_resume() {}
};

inline Task<void> sleep(int seconds) {
    co_await SleepAwaitable{std::chrono::milliseconds(seconds * 1000)};
}

inline Task<void> sleep_ms(int milliseconds) {
    co_await SleepAwaitable{std::chrono::milliseconds(milliseconds)};
}


#endif //CATCH2TESTEXAMPLE_TIMER_HPP