//
// Created by per on 2025-03-02.
//

#ifndef ASYNCIO_HPP
#define ASYNCIO_HPP
#include <variant>

template<typename promise_type>
struct owning_handle {
    owning_handle() = default;

    explicit owning_handle(std::nullptr_t) noexcept : handle_(nullptr) {}

    explicit owning_handle(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}

    owning_handle(const owning_handle &) = delete;
    owning_handle &operator=(const owning_handle &) = delete;

    owning_handle(owning_handle &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    owning_handle &operator=(owning_handle &&other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~owning_handle() {
        if (handle_)
            handle_.destroy();
    }

    promise_type &promise() const {
        assert(handle_);
        return handle_.promise();
    }

    [[nodiscard]] bool done() const {
        assert(handle_);
        return handle_.done();
    }

    void resume() const {
        assert(handle_);
        handle_.resume();
    }

    [[nodiscard]] std::coroutine_handle<promise_type> raw_handle() const noexcept {
        return handle_;
    }

    std::coroutine_handle<promise_type> release() noexcept {
        return std::exchange(handle_, nullptr);
    }

    explicit operator bool() const noexcept {
        return handle_ != nullptr;
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

    struct initial_awaiter {
        bool await_ready() noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<Promise> h) noexcept {
            std::cout << "Init await suspend\n";
        }

        void await_resume() noexcept {
            std::cout << "Init await resume\n";
        }
    };

    struct final_awaiter {
        bool await_ready() noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<Promise> h) noexcept {
            std::cout << "Final: \n";
            std::cout << "Cont: " << h.address() << '\n';
            // The coroutine is now suspended at the final-suspend point.
            // Lookup its continuation in the promise and resume it.
            if (h.promise().continuation) {
                //h.promise().continuation.resume();
            }

            //if (h.promise().continuation)
             //   h.promise().continuation.promise().next = {};
        }

        void await_resume() noexcept {}
    };

    auto initial_suspend() noexcept { return initial_awaiter{}; }
    auto final_suspend() noexcept {
        return final_awaiter{};
    }

    void unhandled_exception() noexcept {
        result = std::current_exception();
    }

    //void return_void() noexcept {}
    void return_value(T&& value) {
        result = std::forward<T>(value);
    }

    std::variant<std::monostate, T, std::exception_ptr> result{};

    std::coroutine_handle<Promise<T>> continuation{};
    std::coroutine_handle<> next{};

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

    struct final_awaiter {
        bool await_ready() noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<Promise> h) noexcept {
            // The coroutine is now suspended at the final-suspend point.
            // Lookup its continuation in the promise and resume it.
            if (h.promise().continuation) {
                //h.promise().continuation.resume();
            }
        }

        void await_resume() noexcept {}
    };

    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept {
        return final_awaiter{};
    }

    void unhandled_exception() noexcept {
        result = std::current_exception();
    }

    void return_void() noexcept {}

    std::variant<std::monostate, std::exception_ptr> result{};

    std::coroutine_handle<Promise<void>> continuation{};
    std::coroutine_handle<> next{};

    void getResult() {
        if (result.index() == 1)
            std::rethrow_exception(std::get<1>(result));
    }
};


template<class T>
struct Task {
    using promise_type = Promise<T>;

    struct Awaiter {
        [[nodiscard]] bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<promise_type> continuation) const noexcept {
            std::cout << "Await Suspend: " << coro_.address() << '\n';
            std::cout << "Cont: " << continuation.address() << '\n';
            // The void-returning version of await_suspend() unconditionally transfers execution back to the
            // caller/resumer of the coroutine when the call to await_suspend() returns, whereas the bool-returning
            // version allows the awaiter object to conditionally resume the coroutine immediately without returning
            // to the caller/resumer.

            // Store the continuation in the task's promise so that the final_suspend()
            // knows to resume this coroutine when the task completes.
            coro_.promise().continuation = continuation;

            continuation.promise().next = coro_;

            std::cout << "Next: " << continuation.promise().next.address() << " <- " << continuation.address() << '\n';

            // Then we resume the task's coroutine, which is currently suspended
            // at the initial-suspend-point (ie. at the open curly brace).

            //std::cout << "Resuming: " << coro_.address() << '\n';
            //coro_.resume();
        }
        void await_resume() const noexcept {
            std::cout << "Await resume: " << coro_.address() << '\n';
            // The return-value of the await_resume() method call becomes the result of the co_await expression.
            // The await_resume() method can also throw an exception in which case the exception propagates out of the
            // co_await expression.
            if (coro_) {
                std::cout << "Next resume: " << coro_.promise().next.address() << '\n';
                //coro_.promise().next.resume();
            }
            coro_.resume();
        }

        explicit Awaiter(std::coroutine_handle<Task::promise_type> h) noexcept : coro_(h) {}

        std::coroutine_handle<Task::promise_type> coro_{};
    };


    auto operator co_await() {

        return Awaiter{handle.raw_handle()};
    }

    explicit Task(std::coroutine_handle<promise_type> h) {
        std::cout << "Task: " << h.address() << '\n';
        handle = static_cast<owning_handle<promise_type>>(h);
    }

    void step() {
        assert(!handle.done());
        std::cout << "Step resume: " << handle.raw_handle().address() << '\n';
        handle.resume();
    }

    auto get() {
        if constexpr (std::is_void_v<T>) {
            return std::monostate{};
        } else {
            return handle.promise().getResult();
        }
    }

    bool done() const { return handle.done(); }

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
