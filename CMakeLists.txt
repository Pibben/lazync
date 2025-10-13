cmake_minimum_required(VERSION 3.14)
project(Catch2TestExample)

set(CMAKE_CXX_STANDARD 23)

add_compile_options(-Wall -Wextra -pedantic -Werror)

enable_testing()

include(FetchContent)
FetchContent_Declare(
        Catch2

        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0 # Update this to the latest stable version
)
FetchContent_MakeAvailable(Catch2)

add_library(lazync INTERFACE)
target_include_directories(lazync INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_main test_main.cpp)

# Link your headers and Catch2 to the test
target_link_libraries(test_main
        PRIVATE
        lazync
        Catch2::Catch2WithMain
)

# Register the test with CMake
add_test(NAME Catch2Tests COMMAND test_main)
