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

        // Wait for the coroutine to actually complete
        while (!handle.done()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

        // Wait for the coroutine to actually complete
        while (!handle.done()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

Task<void> sleep(int seconds) {
    co_await SleepAwaitable{std::chrono::milliseconds(seconds * 1000)};
}

Task<void> sleep_ms(int milliseconds) {
    co_await SleepAwaitable{std::chrono::milliseconds(milliseconds)};
}

// when_all for Tasks - stores completion handlers to keep them alive
template<typename... Tasks>
class WhenAllAwaitable {
public:
    explicit WhenAllAwaitable(Tasks&&... tasks)
        : tasks_(std::forward<Tasks>(tasks)...)
        , state_(std::make_shared<State>()) {
        state_->remaining_count = sizeof...(Tasks);
        state_->completion_handlers.reserve(sizeof...(Tasks));
    }

    bool await_ready() {
        return false;
    }

    void await_suspend(std::coroutine_handle<> awaiting_coro) {
        state_->awaiting = awaiting_coro;

        // Start all tasks
        start_all(std::index_sequence_for<Tasks...>{});
    }

    void await_resume() {}

private:
    struct State {
        std::coroutine_handle<> awaiting;
        std::atomic<size_t> remaining_count{0};
        std::vector<Task<void>> completion_handlers;
    };

    template<size_t... Is>
    void start_all(std::index_sequence<Is...>) {
        (start_one<Is>(), ...);
    }

    template<size_t I>
    void start_one() {
        auto& task = std::get<I>(tasks_);
        auto handle = task.get_handle();

        // Create and store a completion handler
        auto completion_handler = create_completion_coro(state_);
        auto completion_handle = completion_handler.get_handle();
        state_->completion_handlers.push_back(std::move(completion_handler));

        // Set continuation
        handle.promise().continuation = completion_handle;

        // Start the task
        handle.resume();
    }

    static Task<void> create_completion_coro(std::shared_ptr<State> state) {
        struct Awaiter {
            std::shared_ptr<State> state;
            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<>) {
                size_t remaining = --(state->remaining_count);
                if (remaining == 0) {
                    state->awaiting.resume();
                }
            }
            void await_resume() {}
        };

        co_await Awaiter{state};
    }

    std::tuple<Tasks...> tasks_;
    std::shared_ptr<State> state_;
};

template<typename... Tasks>
auto when_all(Tasks&&... tasks) {
    return WhenAllAwaitable<Tasks...>(std::forward<Tasks>(tasks)...);
}
