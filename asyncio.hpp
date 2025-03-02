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

template <class T>
struct Task;

template <class T>
struct Promise {
    friend struct Task<T>;
    Task<T> get_return_object() noexcept {
        return Task{std::coroutine_handle<Promise>::from_promise(*this)};
    }

    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return std::suspend_always{}; }

    void unhandled_exception() noexcept {
        result = std::current_exception();
    }

    //void return_void() noexcept {}
    void return_value(T&& value) {
        result = std::forward<T>(value);
    }

    std::variant<std::monostate, T, std::exception_ptr> result{};

    T&& getResult() {
        if (result.index() == 2)
            std::rethrow_exception(std::get<2>(result));
        return std::move(std::get<1>(result));
    }
};

template <>
struct Promise<void> {
    friend struct Task<void>;
    Task<void> get_return_object() noexcept;

    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return std::suspend_always{}; }

    void unhandled_exception() noexcept {
        result = std::current_exception();
    }

    void return_void() noexcept {}

    std::variant<std::monostate, std::exception_ptr> result{};

    void getResult() {
        if (result.index() == 1)
            std::rethrow_exception(std::get<1>(result));
    }
};


template<class T>
struct Task {
    using promise_type = Promise<T>;
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
            return handle.promise().getResult();
        }
    }

    owning_handle<promise_type> handle;
};

inline Task<void> Promise<void>::get_return_object() noexcept {
    return Task<void>{std::coroutine_handle<Promise>::from_promise(*this)};
}

template<typename... Ts>
auto func(Task<Ts> &... tasks) -> std::tuple<Ts...> {
    return std::make_tuple(tasks.get()...);
}

#endif //ASYNCIO_HPP
