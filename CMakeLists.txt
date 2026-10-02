cmake_minimum_required(VERSION 3.16)

project(
    TodoCLI
    VERSION 2.0.0
    LANGUAGES CXX
)

# -------------------------
# 正式业务库
# -------------------------

add_library(
    todo_lib
    src/task.cpp
    src/task_manager.cpp
)

target_include_directories(
    todo_lib
    PUBLIC
    include
)

target_compile_features(
    todo_lib
    PUBLIC
    cxx_std_17
)

if (
    CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
)
    target_compile_options(
        todo_lib
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()


# -------------------------
# ASan,UBSan
# -------------------------
option(
    ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)
if (
    ENABLE_SANITIZERS
    AND
    CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
)
    add_library(
        sanitizers
        INTERFACE
    )

    target_compile_options(
        sanitizers
        INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )

    target_link_options(
        sanitizers
        INTERFACE
        -fsanitize=address,undefined
    )

    target_link_libraries(
        todo_lib
        PUBLIC
        sanitizers
    )
endif()

# -------------------------
# 正式程序
# -------------------------

add_executable(
    todo
    src/main.cpp
)

target_link_libraries(
    todo
    PRIVATE
    todo_lib
)

# -------------------------
# GoogleTest
# -------------------------

include(FetchContent)

FetchContent_Declare(
    googletest
    URL
    https://github.com/google/googletest/archive/refs/tags/v1.18.0.zip
)

FetchContent_MakeAvailable(googletest)

# -------------------------
# 测试
# -------------------------

enable_testing()

add_executable(
    todo_tests
    tests/task_test.cpp
    tests/task_manager_test.cpp
)

target_link_libraries(
    todo_tests
    PRIVATE
    todo_lib
    GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(todo_tests)