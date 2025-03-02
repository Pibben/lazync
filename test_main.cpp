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

    auto v = task.get();
    REQUIRE(v == 2);
}

TEST_CASE("Exceptions", "[test]") {
    auto throws = [&] -> Task<int> {
        throw std::exception();
        co_return 5;
    };

    auto task3 = throws();
    task3.step();
    REQUIRE_THROWS(task3.get());
}
