#define CATCH_CONFIG_MAIN  // This tells Catch to generate a main() function
#include <catch2/catch_all.hpp>

#include <coroutine>
#include <iostream>

#include "asyncio.hpp"

TEST_CASE("Test function", "[test]") {
    int state = 0;

    auto foo = [&] -> Task<int> {
        state = state * 10 + 3;
        co_return 1;
    };

    auto bar = [&] -> Task<int> {
        state = state * 10 + 2;
        co_await foo();
        state = state * 10 + 4;
        co_return 1;
    };

    auto coro = [&] -> Task<int> {
        state = state * 10 + 1;
        co_await bar();
        state = state * 10 + 5;
        co_return 2;
    };

    auto task = coro();
    REQUIRE(state == 0);
    task.step();
    REQUIRE(state == 1);
    task.step();
    REQUIRE(state == 125);
    REQUIRE(task.done());

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

TEST_CASE("Void", "[test]") {
    auto voidreturn = [&] -> Task<void> {;
        co_return;
    };

    auto task3 = voidreturn();
    task3.step();
}
