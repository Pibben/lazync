#define CATCH_CONFIG_MAIN  // This tells Catch to generate a main() function
#include <catch2/catch_all.hpp>

#include <coroutine>
#include <iostream>

#include "asyncio.hpp"

TEST_CASE("Test function", "[test]") {
    int state = 0;
    auto bar = [&] -> Task<int> {
        state = 2;
        co_return 1;
    };

    auto coro = [&] -> Task<int> {
        state = 1;
        co_await bar();
        state = 3;
        co_return 2;
    };

    auto task = coro();
    REQUIRE(state == 0);
    task.step();
    REQUIRE(state == 1);
    task.step();
    REQUIRE(state == 3);

    auto task2 = coro();

    auto t = func(task, task2);
}
