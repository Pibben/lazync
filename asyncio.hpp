#include <coroutine>
#include <exception>
#include <utility>
#include <queue>
#include <functional>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

// Forward declarations
class Scheduler;
Scheduler& get_scheduler();

// Task implementation
template<typename T = void>
class Task {
public:
    struct promise_type {
        T value;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;
        Scheduler* scheduler = nullptr;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }

        struct final_awaiter {
            bool await_ready() noexcept { return false; }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                if (h.promise().continuation) {
                    return h.promise().continuation;
                }
                return std::noop_coroutine();
            }

            void await_resume() noexcept {}
        };

        final_awaiter final_suspend() noexcept { return {}; }

        void return_value(T val) {
            value = std::move(val);
        }

        void unhandled_exception() {
            exception = std::current_exception();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type h) : handle(h) {}

    Task(Task&& other) noexcept : handle(std::exchange(other.handle, {})) {}

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }

    ~Task() {
        if (handle) handle.destroy();
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    T get() {
        if (!handle.done()) {
            handle.resume();
        }
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
        return std::move(handle.promise().value);
    }

    bool done() const { return handle.done(); }

    // Awaiter for co_await support
    struct awaiter {
        handle_type coro;

        bool await_ready() {
            return coro.done();
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) {
            coro.promise().continuation = awaiting;
            return coro;
        }

        T await_resume() {
            if (coro.promise().exception) {
                std::rethrow_exception(coro.promise().exception);
            }
            return std::move(coro.promise().value);
        }
    };

    awaiter operator co_await() {
        return awaiter{handle};
    }

    handle_type get_handle() { return handle; }

private:
    handle_type handle;
};

// Specialization for void
template<>
class Task<void> {
public:
    struct promise_type {
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;
        Scheduler* scheduler = nullptr;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }

        struct final_awaiter {
            bool await_ready() noexcept { return false; }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                if (h.promise().continuation) {
                    return h.promise().continuation;
                }
                return std::noop_coroutine();
            }

            void await_resume() noexcept {}
        };

        final_awaiter final_suspend() noexcept { return {}; }

        void return_void() {}

        void unhandled_exception() {
            exception = std::current_exception();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type h) : handle(h) {}

    Task(Task&& other) noexcept : handle(std::exchange(other.handle, {})) {}

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }

    ~Task() {
        if (handle) handle.destroy();
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    void get() {
        if (!handle.done()) {
            handle.resume();
        }
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
    }

    bool done() const { return handle.done(); }

    // Awaiter for co_await support
    struct awaiter {
        handle_type coro;

        bool await_ready() {
            return coro.done();
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) {
            coro.promise().continuation = awaiting;
            return coro;
        }

        void await_resume() {
            if (coro.promise().exception) {
                std::rethrow_exception(coro.promise().exception);
            }
        }
    };

    awaiter operator co_await() {
        return awaiter{handle};
    }

    handle_type get_handle() { return handle; }

private:
    handle_type handle;
};

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
Scheduler& get_scheduler() {
    static Scheduler scheduler;
    return scheduler;
}

// Sleep awaitable that uses the scheduler
struct SleepAwaitable {
    std::chrono::milliseconds duration;

    bool await_ready() { return duration.count() == 0; }

    void await_suspend(std::coroutine_handle<> coro) {
        get_scheduler().schedule_after(coro, duration);
    }

    void await_resume() {}
};

// Sleep function
SleepAwaitable sleep(int seconds) {
    return SleepAwaitable{std::chrono::milliseconds(seconds * 1000)};
}

SleepAwaitable sleep_ms(int milliseconds) {
    return SleepAwaitable{std::chrono::milliseconds(milliseconds)};
}

// when_all for any awaitables (including SleepAwaitable and Task)
template<typename... Awaitables>
class WhenAllAwaitable {
public:
    explicit WhenAllAwaitable(Awaitables... awaitables)
        : awaitables_(std::make_tuple(awaitables...)) {}

    bool await_ready() {
        return false;
    }

    void await_suspend(std::coroutine_handle<> awaiting_coro) {
        awaiting_ = awaiting_coro;
        remaining_count_ = sizeof...(Awaitables);

        // Create and store wrapper tasks to keep them alive
        start_all(std::index_sequence_for<Awaitables...>{});
    }

    void await_resume() {
        // Clean up wrapper tasks
        for (auto* task : wrapper_tasks_) {
            delete task;
        }
        wrapper_tasks_.clear();
    }

private:
    template<size_t... Is>
    void start_all(std::index_sequence<Is...>) {
        (start_one<Is>(), ...);
    }

    template<size_t I>
    void start_one() {
        // Create a wrapper task that awaits the awaitable
        auto* task_ptr = new Task<void>(create_wrapper<I>());
        wrapper_tasks_.push_back(task_ptr);
        task_ptr->get_handle().resume();
    }

    template<size_t I>
    Task<void> create_wrapper() {
        co_await std::get<I>(awaitables_);
        on_complete();
    }

    void on_complete() {
        if (--remaining_count_ == 0) {
            awaiting_.resume();
        }
    }

    std::tuple<Awaitables...> awaitables_;
    std::coroutine_handle<> awaiting_;
    std::atomic<size_t> remaining_count_{0};
    std::vector<Task<void>*> wrapper_tasks_;
};

template<typename... Awaitables>
auto when_all(Awaitables... awaitables) {
    return WhenAllAwaitable<Awaitables...>(awaitables...);
}
#if 0
// when_all implementation
template<typename... Tasks>
class WhenAllAwaitable {
public:
    explicit WhenAllAwaitable(Tasks&&... tasks)
        : tasks_(std::forward<Tasks>(tasks)...) {}

    bool await_ready() {
        return false;  // Always suspend to start all tasks
    }

    void await_suspend(std::coroutine_handle<> awaiting_coro) {
        awaiting_ = awaiting_coro;
        remaining_count_ = sizeof...(Tasks);

        // Start all tasks
        start_all(std::index_sequence_for<Tasks...>{});
    }

    auto await_resume() {
        // Return a tuple of results
        return get_results(std::index_sequence_for<Tasks...>{});
    }

private:
    template<size_t... Is>
    void start_all(std::index_sequence<Is...>) {
        // Start each task with a callback
        (start_task<Is>(), ...);
    }

    template<size_t I>
    void start_task() {
        auto& task = std::get<I>(tasks_);
        auto handle = task.get_handle();

        // Set continuation to our completion handler
        handle.promise().continuation =
            std::coroutine_handle<CompletionPromise>::from_promise(
                *new CompletionPromise{this}
            );

        // Start the task
        handle.resume();
    }

    void on_task_complete() {
        if (--remaining_count_ == 0) {
            // All tasks done, resume the awaiting coroutine
            awaiting_.resume();
        }
    }

    template<size_t... Is>
    auto get_results(std::index_sequence<Is...>) {
        return std::make_tuple(get_result<Is>()...);
    }

    template<size_t I>
    auto get_result() {
        auto& task = std::get<I>(tasks_);
        auto& promise = task.get_handle().promise();

        if (promise.exception) {
            std::rethrow_exception(promise.exception);
        }

        if constexpr (std::is_void_v<decltype(task.get_handle().promise().value)>) {
            return;
        } else {
            return std::move(promise.value);
        }
    }

    // Helper promise for completion callbacks
    struct CompletionPromise {
        WhenAllAwaitable* parent;

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept {
            parent->on_task_complete();
            return {};
        }
        void return_void() {}
        void unhandled_exception() {}
        auto get_return_object() { return std::coroutine_handle<CompletionPromise>::from_promise(*this); }
    };

    std::tuple<Tasks...> tasks_;
    std::coroutine_handle<> awaiting_;
    std::atomic<size_t> remaining_count_{0};
};

template<typename... Tasks>
auto when_all(Tasks&&... tasks) {
    return WhenAllAwaitable<Tasks...>(std::forward<Tasks>(tasks)...);
}
#endif
