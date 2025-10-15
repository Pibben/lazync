#include "task.hpp"

#include <atomic>

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
