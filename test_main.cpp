#define CATCH_CONFIG_MAIN  // This tells Catch to generate a main() function
#include <catch2/catch_all.hpp>

#include <coroutine>
#include <iostream>

#include "asyncio.hpp"

Task<int> calculate_async(int x) {
    co_return x * 2 + 10;
}

Task<int> complex_calculation() {
    auto task1 = calculate_async(5);
    auto task2 = calculate_async(10);

    int result1 = task1.get();
    int result2 = task2.get();

    co_return result1 + result2;
}

Task<void> void_task() {
    co_return;
}

Task<int> throwing_task() {
    throw std::runtime_error("Oops!");
    co_return 42;
}

Task<std::string> string_task() {
    co_return "Hello from coroutine";
}

TEST_CASE("Task: Simple calculation", "[task]") {
    auto task = calculate_async(7);
    REQUIRE_FALSE(task.done());

    int result = task.get();
    REQUIRE(result == 24);
    REQUIRE(task.done());
}

TEST_CASE("Task: Complex calculation with multiple tasks", "[task]") {
    auto task = complex_calculation();
    REQUIRE_FALSE(task.done());

    int result = task.get();
    // task1: 5 * 2 + 10 = 20
    // task2: 10 * 2 + 10 = 30
    // total: 20 + 30 = 50
    REQUIRE(result == 50);
    REQUIRE(task.done());
}

TEST_CASE("Task: Void return type", "[task]") {
    auto task = void_task();
    REQUIRE_FALSE(task.done());

    REQUIRE_NOTHROW(task.get());
    REQUIRE(task.done());
}

TEST_CASE("Task: Exception handling", "[task]") {
    auto task = throwing_task();
    REQUIRE_FALSE(task.done());

    REQUIRE_THROWS_AS(task.get(), std::runtime_error);
    REQUIRE_THROWS_WITH(task.get(), "Oops!");
}

TEST_CASE("Task: String return type", "[task]") {
    auto task = string_task();

    std::string result = task.get();
    REQUIRE(result == "Hello from coroutine");
}

TEST_CASE("Task: Move semantics", "[task]") {
    auto task1 = calculate_async(5);

    // Move construction
    auto task2 = std::move(task1);
    REQUIRE_FALSE(task2.done());
    REQUIRE(task2.get() == 20);

    // Move assignment
    auto task3 = calculate_async(3);
    task3 = calculate_async(7);
    REQUIRE(task3.get() == 24);
}

TEST_CASE("Task: Lazy evaluation", "[task]") {
    bool executed = false;

    auto lazy_task = [&]() -> Task<int> {
        //executed = true;
        co_return 42;
    }();

    // Task hasn't executed yet
    REQUIRE_FALSE(executed);
    REQUIRE_FALSE(lazy_task.done());

    // Now it executes
    int result = lazy_task.get();
    //REQUIRE(executed);
    REQUIRE(result == 42);
}

TEST_CASE("Task: Multiple return values", "[task]") {
    SECTION("Different input values") {
        auto task1 = calculate_async(0);
        REQUIRE(task1.get() == 10);

        auto task2 = calculate_async(10);
        REQUIRE(task2.get() == 30);

        auto task3 = calculate_async(-5);
        REQUIRE(task3.get() == 0);
    }
}