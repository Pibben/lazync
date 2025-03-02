//
// Created by per on 2025-03-02.
//

#ifndef ASYNCIO_HPP
#define ASYNCIO_HPP
#include <variant>

template<typename promise_type>
struct owning_handle {
    owning_handle() = default;

    explicit owning_handle(std::nullptr_t) : handle_(nullptr) {
    }

    explicit owning_handle(std::coroutine_handle<promise_type> handle) : handle_(std::move(handle)) {
    }

    owning_handle(const owning_handle<promise_type> &) = delete;

    owning_handle(owning_handle<promise_type> &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {
    }

    owning_handle<promise_type> &operator=(const owning_handle<promise_type> &) = delete;

    owning_handle<promise_type> &operator=(owning_handle<promise_type> &&other) noexcept {
        handle_ = std::exchange(other.handle_, nullptr);
        return *this;
    }

    promise_type &promise() const {
        return handle_.promise();
    }

    [[nodiscard]] bool done() const {
        assert(handle_ != nullptr);
        return handle_.done();
    }

    void resume() const {
        assert(handle_ != nullptr);
        return handle_.resume();
    }

    [[nodiscard]] std::coroutine_handle<promise_type> raw_handle() const {
        return handle_;
    }

    ~owning_handle() {
        if (handle_ != nullptr)
            handle_.destroy();
    }

private:
    std::coroutine_handle<promise_type> handle_{};
};

template<class T>
struct Task {
    struct promise_type {
        Task get_return_object() noexcept {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        auto initial_suspend() noexcept { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }

        void unhandled_exception() noexcept {
            /* TODO */
        }

        //void return_void() noexcept {}
        void return_value(int) { /* TODO */ }
    };

    auto operator co_await() {
        struct Awaiter {
            [[nodiscard]] bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<>) const noexcept {
                // The void-returning version of await_suspend() unconditionally transfers execution back to the
                // caller/resumer of the coroutine when the call to await_suspend() returns, whereas the bool-returning
                // version allows the awaiter object to conditionally resume the coroutine immediately without returning
                // to the caller/resumer.
            }
            void await_resume() const noexcept {
                // The return-value of the await_resume() method call becomes the result of the co_await expression.
                // The await_resume() method can also throw an exception in which case the exception propagates out of the
                // co_await expression.
            }
        };

        return Awaiter{};
    }

    explicit Task(std::coroutine_handle<promise_type> h) {
        handle = static_cast<owning_handle<promise_type>>(h);
    }

    void step() {
        assert(!handle.done());
        handle.resume();
    }

    auto get() {
        if constexpr (std::is_void_v<T>) {
            return std::monostate{};
        } else {
            return 5;
        }
    }

    owning_handle<promise_type> handle;
};

template<typename... Ts>
auto func(Task<Ts> &... tasks) -> std::tuple<Ts...> {
    return std::make_tuple(tasks.get()...);
}

#endif //ASYNCIO_HPP
