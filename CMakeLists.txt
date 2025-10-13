cmake_minimum_required(VERSION 3.14)
project(Catch2TestExample)

set(CMAKE_CXX_STANDARD 23)

add_compile_options(-Wall -Wextra -pedantic -Werror)

# Enable testing in CMake
enable_testing()

# Fetch Catch2 if not found
include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0 # Update this to the latest stable version
)
FetchContent_MakeAvailable(Catch2)

# Add the test executable
add_executable(test_main test_main.cpp
        utils.hpp
        scheduler.hpp
        task.hpp
        timer.hpp)

# Link Catch2 with the test executable
target_link_libraries(test_main PRIVATE Catch2::Catch2WithMain)

# Register the test with CMake
add_test(NAME Catch2Tests COMMAND test_main)
