#include <coroutine>
#include <exception>
#include <utility>
#include <string>

// Task implementation
template<typename T = void>
class Task {
public:
    struct promise_type {
        T value;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

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

private:
    handle_type handle;
};
