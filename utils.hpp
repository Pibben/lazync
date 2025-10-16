#include "task.hpp"

#include <atomic>

// Helper to extract return type from Task<T>
template<typename T>
struct task_return_type;

template<typename T>
struct task_return_type<Task<T>> {
    using type = T;
};

template<typename T>
using task_return_type_t = typename task_return_type<T>::type;

// when_all for Tasks - all tasks must return a value (non-void)
template<typename... Tasks>
class WhenAllAwaitable {
public:
    using ReturnTuple = std::tuple<task_return_type_t<std::remove_reference_t<Tasks>>...>;

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

    ReturnTuple await_resume() {
        return std::move(state_->results);
    }

private:
    struct State {
        std::coroutine_handle<> awaiting;
        std::atomic<size_t> remaining_count{0};
        std::vector<Task<void>> completion_handlers;
        ReturnTuple results;
    };

    template<size_t... Is>
    void start_all(std::index_sequence<Is...>) {
        (start_one<Is>(), ...);
    }

    template<size_t I>
    void start_one() {
        auto& task = std::get<I>(tasks_);
        auto handle = task.get_handle();

        // Create and store a completion handler that captures the task result
        auto completion_handler = create_completion_coro<I>(state_, handle);
        auto completion_handle = completion_handler.get_handle();
        state_->completion_handlers.push_back(std::move(completion_handler));

        // Set continuation
        handle.promise().continuation = completion_handle;

        // Start the task
        handle.resume();
    }

    template<size_t I>
    static Task<void> create_completion_coro(
        std::shared_ptr<State> state,
        std::coroutine_handle<typename std::remove_reference_t<
            std::tuple_element_t<I, std::tuple<Tasks...>>
        >::promise_type> task_handle) {

        using TaskType = std::remove_reference_t<std::tuple_element_t<I, std::tuple<Tasks...>>>;
        //using ReturnType = task_return_type_t<TaskType>;

        struct Awaiter {
            std::shared_ptr<State> state;
            std::coroutine_handle<typename TaskType::promise_type> task_handle;

            bool await_ready() { return false; }

            void await_suspend(std::coroutine_handle<>) {
                // Extract and store the result from the completed task
                std::get<I>(state->results) = std::move(task_handle.promise().value);

                // Decrement counter and resume awaiting coroutine if this was the last task
                size_t remaining = --(state->remaining_count);
                if (remaining == 0) {
                    state->awaiting.resume();
                }
            }

            void await_resume() {}
        };

        co_await Awaiter{state, task_handle};
    }

    std::tuple<Tasks...> tasks_;
    std::shared_ptr<State> state_;
};

// Specialization for all-void tasks (optional, for cleaner code)
template<typename... Tasks>
    requires (std::is_void_v<task_return_type_t<std::remove_reference_t<Tasks>>> && ...)
class WhenAllAwaitableVoid {
public:
    explicit WhenAllAwaitableVoid(Tasks&&... tasks)
        : tasks_(std::forward<Tasks>(tasks)...)
        , state_(std::make_shared<State>()) {
        state_->remaining_count = sizeof...(Tasks);
        state_->completion_handlers.reserve(sizeof...(Tasks));
    }

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> awaiting_coro) {
        state_->awaiting = awaiting_coro;
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

        auto completion_handler = create_completion_coro(state_);
        auto completion_handle = completion_handler.get_handle();
        state_->completion_handlers.push_back(std::move(completion_handler));

        handle.promise().continuation = completion_handle;
        handle.resume();
    }

    static Task<void> create_completion_coro(std::shared_ptr<State> state) {
        struct Awaiter {
            std::shared_ptr<State> state;
            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<>) {
                if (--(state->remaining_count) == 0) {
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

// when_all factory function - dispatches to appropriate implementation
template<typename... Tasks>
auto when_all(Tasks&&... tasks) {
    constexpr bool all_void = (std::is_void_v<task_return_type_t<std::remove_reference_t<Tasks>>> && ...);

    if constexpr (all_void) {
        return WhenAllAwaitableVoid<Tasks...>(std::forward<Tasks>(tasks)...);
    } else {
        return WhenAllAwaitable<Tasks...>(std::forward<Tasks>(tasks)...);
    }
}
