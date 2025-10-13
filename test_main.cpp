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

// Coroutines using co_await
Task<int> async_add(int a, int b) {
    co_return a + b;
}

Task<int> chained_calculation() {
    int result1 = co_await async_add(5, 10);
    int result2 = co_await async_add(result1, 20);
    co_return result2;
}

Task<int> parallel_style_calculation() {
    auto task1 = async_add(5, 10);
    auto task2 = async_add(3, 7);

    int result1 = co_await task1;
    int result2 = co_await task2;

    co_return result1 + result2;
}

Task<void> async_void_operation() {
    co_await void_task();
    co_return;
}

Task<int> async_exception_propagation() {
    co_await throwing_task();
    co_return 999;  // Never reached
}

Task<int> test_await_resume(int* counter) {
    (*counter)++;
    co_return 42;
}

Task<int> verify_await_resume_called(int* counter) {
    int result = co_await test_await_resume(counter);
    (*counter) += 100;  // This proves await_resume was called and returned the value
    co_return result;
}

// Nested Task-returning functions
Task<int> level1_task(int value) {
    co_return value * 2;
}

Task<int> level2_task(int value) {
    int result = co_await level1_task(value);
    co_return result + 10;
}

Task<int> level3_task(int value) {
    int result = co_await level2_task(value);
    co_return result * 3;
}

Task<int> level4_task(int value) {
    int result = co_await level3_task(value);
    co_return result + 5;
}

// Multiple nested awaits in sequence
Task<int> deeply_nested_sequential() {
    int a = co_await level1_task(5);   // 5 * 2 = 10
    int b = co_await level1_task(a);   // 10 * 2 = 20
    int c = co_await level2_task(b);   // (20 * 2) + 10 = 50
    int d = co_await level3_task(c);   // ((50 * 2) + 10) * 3 = 330
    co_return d;
}

// Nested with multiple parallel tasks
Task<int> nested_parallel() {
    auto task1 = level2_task(5);  // (5 * 2) + 10 = 20
    auto task2 = level2_task(3);  // (3 * 2) + 10 = 16

    int result1 = co_await task1;
    int result2 = co_await task2;

    // Now use those results in another nested call
    int final_result = co_await level3_task(result1 + result2);  // ((36 * 2) + 10) * 3 = 246
    co_return final_result;
}

// Tests
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

Task<int> lazy_evaluation_task(bool* executed) {
    *executed = true;
    co_return 42;
}

TEST_CASE("Task: Lazy evaluation", "[task]") {
    bool executed = false;

    auto lazy_task = lazy_evaluation_task(&executed);

    // Task hasn't executed yet
    REQUIRE_FALSE(executed);
    REQUIRE_FALSE(lazy_task.done());

    // Now it executes
    int result = lazy_task.get();
    REQUIRE(executed);
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

TEST_CASE("Task: co_await chained calculation", "[task][await]") {
    auto task = chained_calculation();
    // result1 = 5 + 10 = 15
    // result2 = 15 + 20 = 35
    int result = task.get();
    REQUIRE(result == 35);
}

TEST_CASE("Task: co_await parallel style", "[task][await]") {
    auto task = parallel_style_calculation();
    // task1 = 5 + 10 = 15
    // task2 = 3 + 7 = 10
    // total = 15 + 10 = 25
    int result = task.get();
    REQUIRE(result == 25);
}

TEST_CASE("Task: co_await void task", "[task][await]") {
    auto task = async_void_operation();
    REQUIRE_NOTHROW(task.get());
}

TEST_CASE("Task: co_await exception propagation", "[task][await]") {
    auto task = async_exception_propagation();
    REQUIRE_THROWS_AS(task.get(), std::runtime_error);
    REQUIRE_THROWS_WITH(task.get(), "Oops!");
}

TEST_CASE("Task: co_await vs get() comparison", "[task][await]") {
    SECTION("Using co_await") {
        auto task = chained_calculation();
        REQUIRE(task.get() == 35);
    }

    SECTION("Using get() only") {
        auto task = complex_calculation();
        REQUIRE(task.get() == 50);
    }
}

TEST_CASE("Task: await_resume is called", "[task][await]") {
    int counter = 0;
    auto task = verify_await_resume_called(&counter);

    int result = task.get();

    // Counter should be 1 (from test_await_resume) + 100 (after await_resume)
    REQUIRE(counter == 101);
    REQUIRE(result == 42);
}

TEST_CASE("Task: Nested co_await - single chain", "[task][await][nested]") {
    SECTION("2 levels deep") {
        auto task = level2_task(5);
        // (5 * 2) + 10 = 20
        REQUIRE(task.get() == 20);
    }

    SECTION("3 levels deep") {
        auto task = level3_task(5);
        // ((5 * 2) + 10) * 3 = 60
        REQUIRE(task.get() == 60);
    }

    SECTION("4 levels deep") {
        auto task = level4_task(5);
        // (((5 * 2) + 10) * 3) + 5 = 65
        REQUIRE(task.get() == 65);
    }
}

TEST_CASE("Task: Deeply nested sequential awaits", "[task][await][nested]") {
    auto task = deeply_nested_sequential();
    // a = 5 * 2 = 10
    // b = 10 * 2 = 20
    // c = (20 * 2) + 10 = 50
    // d = ((50 * 2) + 10) * 3 = 330
    REQUIRE(task.get() == 330);
}

TEST_CASE("Task: Nested with parallel tasks", "[task][await][nested]") {
    auto task = nested_parallel();
    // task1 = (5 * 2) + 10 = 20
    // task2 = (3 * 2) + 10 = 16
    // sum = 36
    // final = ((36 * 2) + 10) * 3 = 246
    REQUIRE(task.get() == 246);
}


Task<void> parallel_sleeps() {
    co_await sleep_ms(200);
    co_await sleep_ms(200);
}

Task<void> sequential_operations() {
    co_await sleep_ms(100);
    co_await sleep_ms(100);
    co_await sleep_ms(100);
}

Task<int> compute_with_delay() {
    co_await sleep_ms(100);
    co_return 42;
}

Task<void> mixed_operations() {
    int result = co_await compute_with_delay();
    co_await sleep_ms(100);
}

// TRUE PARALLEL EXAMPLE using when_all
Task<void> truly_parallel_sleeps() {
    co_await when_all(sleep_ms(200), sleep_ms(200));
}

Task<void> parallel_multiple_sleeps() {
    co_await when_all(sleep_ms(100), sleep_ms(100), sleep_ms(100), sleep_ms(100));
}

Task<int> parallel_compute() {
    co_await when_all(sleep_ms(100), sleep_ms(150));
    co_return 99;
}

TEST_CASE("Scheduler: Sequential sleeps take cumulative time", "[scheduler][sequential]") {
    auto start = std::chrono::steady_clock::now();

    auto task = parallel_sleeps();
    task.get();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Two 200ms sleeps should take ~400ms
    REQUIRE(duration >= 380);
    REQUIRE(duration < 450);
}

TEST_CASE("Scheduler: Multiple sequential operations", "[scheduler][sequential]") {
    auto start = std::chrono::steady_clock::now();

    auto task = sequential_operations();
    task.get();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Three 100ms sleeps should take ~300ms
    REQUIRE(duration >= 280);
    REQUIRE(duration < 350);
}

TEST_CASE("Scheduler: Compute with delay returns correct value", "[scheduler]") {
    auto start = std::chrono::steady_clock::now();

    auto task = compute_with_delay();
    int result = task.get();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    REQUIRE(result == 42);
    REQUIRE(duration >= 90);
    REQUIRE(duration < 150);
}

TEST_CASE("Scheduler: when_all runs operations in parallel", "[scheduler][parallel][when_all]") {
    auto start = std::chrono::steady_clock::now();

    auto task = truly_parallel_sleeps();
    task.get();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Two parallel 200ms sleeps should take ~200ms, NOT 400ms
    REQUIRE(duration >= 180);
    REQUIRE(duration < 250);
}

TEST_CASE("Scheduler: when_all with multiple operations", "[scheduler][parallel][when_all]") {
    auto start = std::chrono::steady_clock::now();

    auto task = parallel_multiple_sleeps();
    task.get();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Four parallel 100ms sleeps should take ~100ms, NOT 400ms
    REQUIRE(duration >= 90);
    REQUIRE(duration < 150);
}

TEST_CASE("Scheduler: when_all returns after longest operation", "[scheduler][parallel][when_all]") {
    auto start = std::chrono::steady_clock::now();

    auto task = parallel_compute();
    int result = task.get();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    REQUIRE(result == 99);
    // Should wait for the longest (150ms), not the sum (250ms)
    REQUIRE(duration >= 140);
    REQUIRE(duration < 200);
}

TEST_CASE("Scheduler: Comparison of sequential vs parallel", "[scheduler][comparison]") {
    SECTION("Sequential execution") {
        auto start = std::chrono::steady_clock::now();

        auto task = []() -> Task<void> {
            co_await sleep_ms(100);
            co_await sleep_ms(100);
        }();
        task.get();

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        REQUIRE(duration >= 190);  // ~200ms
    }

    SECTION("Parallel execution") {
        auto start = std::chrono::steady_clock::now();

        auto task = []() -> Task<void> {
            co_await when_all(sleep_ms(100), sleep_ms(100));
        }();
        task.get();

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        REQUIRE(duration < 150);  // ~100ms
    }
}

TEST_CASE("Scheduler: Mixed sequential and parallel operations", "[scheduler][mixed]") {
    auto start = std::chrono::steady_clock::now();

    auto task = []() -> Task<void> {
        // First: parallel 100ms operations
        co_await when_all(sleep_ms(100), sleep_ms(100));

        // Then: sequential 100ms operation
        co_await sleep_ms(100);

        // Finally: parallel again
        co_await when_all(sleep_ms(50), sleep_ms(50), sleep_ms(50));
    }();
    task.get();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Total: 100 (parallel) + 100 (sequential) + 50 (parallel) = 250ms
    REQUIRE(duration >= 230);
    REQUIRE(duration < 300);
}

TEST_CASE("Scheduler: when_all with different durations", "[scheduler][parallel][when_all]") {
    auto start = std::chrono::steady_clock::now();

    auto task = []() -> Task<void> {
        co_await when_all(sleep_ms(50), sleep_ms(100), sleep_ms(150));
    }();
    task.get();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should take as long as the longest operation (150ms)
    REQUIRE(duration >= 140);
    REQUIRE(duration < 200);
}
