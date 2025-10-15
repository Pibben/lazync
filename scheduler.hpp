//
// Created by per on 2025-10-13.
//

#ifndef CATCH2TESTEXAMPLE_SCHEDULER_H
#define CATCH2TESTEXAMPLE_SCHEDULER_H

#include "task.hpp"

#include <condition_variable>
#include <coroutine>
#include <queue>
#include <uv.h>

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

    Scheduler() {
        uv_loop_init(&loop);
    }

    ~Scheduler() {
        uv_loop_close(&loop);
    }

    void schedule_after(std::coroutine_handle<> coro, std::chrono::milliseconds delay) {
        uv_timer_t timer;
        uv_timer_init(&loop, &timer);
        timer.data = coro.address();
        uv_timer_start(&timer, timer_cb, delay.count(), 0);
    }

    static void timer_cb(uv_timer_t *handle) {
        std::coroutine_handle<> coro = std::coroutine_handle<>::from_address(handle->data);
        coro.resume();
    }

    template <class T>
    T schedule(const Task<T>& task) {
        uv_async_t async;
        uv_async_init(&loop, &async, [](uv_async_t* async) {
            auto thisHandle = std::coroutine_handle<>::from_address(async->data);
            if (!thisHandle.done()) {
                thisHandle.resume();
            }
            uv_close(reinterpret_cast<uv_handle_t*>(async), nullptr);
        });

        auto handle = task.get_handle();
        async.data = handle.address();

        uv_async_send(&async);

        uv_run(&loop, UV_RUN_DEFAULT);

        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }

        return std::move(handle.promise().value);
    }

    void schedule(const Task<void>& task) {
        uv_async_t async;
        uv_async_init(&loop, &async, [](uv_async_t* async) {
            auto thisHandle = std::coroutine_handle<>::from_address(async->data);
            if (!thisHandle.done()) {
                thisHandle.resume();
            }
            uv_close(reinterpret_cast<uv_handle_t*>(async), nullptr);
        });

        async.data = task.get_handle().address();

        uv_async_send(&async);

        uv_run(&loop, UV_RUN_DEFAULT);
    }

private:
    uv_loop_t loop;
};

// Global scheduler
inline Scheduler& get_scheduler() {
    static Scheduler scheduler;
    return scheduler;
}


#endif //CATCH2TESTEXAMPLE_SCHEDULER_H