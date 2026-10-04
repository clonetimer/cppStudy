cmake_minimum_required(VERSION 3.16)

project(
    ThreadPool
    VERSION 1.0.0
    LANGUAGES CXX
)

# --------------------------------------------------
# Global options
# --------------------------------------------------

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(
    BUILD_TESTS
    "Build unit tests"
    ON
)

option(
    BUILD_BENCHMARKS
    "Build simple benchmarks"
    ON
)

option(
    ENABLE_ASAN_UBSAN
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

option(
    ENABLE_TSAN
    "Enable ThreadSanitizer"
    OFF
)

if (ENABLE_ASAN_UBSAN AND ENABLE_TSAN)
    message(
        FATAL_ERROR
        "ASan/UBSan and TSan must use separate builds."
    )
endif()

# --------------------------------------------------
# Threads
# --------------------------------------------------

find_package(
    Threads
    REQUIRED
)

# --------------------------------------------------
# Compiler warnings
# --------------------------------------------------

add_library(
    project_warnings
    INTERFACE
)

if (MSVC)
    target_compile_options(
        project_warnings
        INTERFACE
        /W4
    )
elseif (
    CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
)
    target_compile_options(
        project_warnings
        INTERFACE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# --------------------------------------------------
# Sanitizers
# --------------------------------------------------

add_library(
    project_sanitizers
    INTERFACE
)

if (
    CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
)

    if (ENABLE_ASAN_UBSAN)
        target_compile_options(
            project_sanitizers
            INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )

        target_link_options(
            project_sanitizers
            INTERFACE
            -fsanitize=address,undefined
        )
    endif()

    if (ENABLE_TSAN)
        target_compile_options(
            project_sanitizers
            INTERFACE
            -fsanitize=thread
            -fno-omit-frame-pointer
        )

        target_link_options(
            project_sanitizers
            INTERFACE
            -fsanitize=thread
        )
    endif()

endif()

# --------------------------------------------------
# ThreadPool library
# --------------------------------------------------

add_library(
    thread_pool
    src/thread_pool.cpp
)

target_include_directories(
    thread_pool
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    thread_pool
    PUBLIC
    cxx_std_17
)

target_link_libraries(
    thread_pool
    PUBLIC
    Threads::Threads
    project_warnings
    project_sanitizers
)

# --------------------------------------------------
# Demo executable
# --------------------------------------------------

add_executable(
    thread_pool_demo
    src/main.cpp
)

target_link_libraries(
    thread_pool_demo
    PRIVATE
    thread_pool
)

# --------------------------------------------------
# Tests
# --------------------------------------------------

if (BUILD_TESTS)

    include(FetchContent)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY
        https://github.com/google/googletest.git
        GIT_TAG
        v1.18.0
        GIT_SHALLOW
        TRUE
    )

    # Prevent installing GoogleTest together with this project.
    set(
        INSTALL_GTEST
        OFF
        CACHE BOOL ""
        FORCE
    )

    FetchContent_MakeAvailable(
        googletest
    )

    enable_testing()

    add_executable(
        thread_pool_tests
        tests/thread_pool_test.cpp
    )

    target_link_libraries(
        thread_pool_tests
        PRIVATE
        thread_pool
        GTest::gtest_main
    )

    include(GoogleTest)

    # Important:
    # Do not execute the test binary during the build phase.
    # This avoids the TSan discovery problem encountered earlier.
    gtest_discover_tests(
        thread_pool_tests
        DISCOVERY_MODE PRE_TEST
    )

endif()

# --------------------------------------------------
# Simple benchmark
# --------------------------------------------------

if (BUILD_BENCHMARKS)

    add_executable(
        thread_pool_benchmark
        benchmark/thread_pool_benchmark.cpp
    )

    target_link_libraries(
        thread_pool_benchmark
        PRIVATE
        thread_pool
    )

endif()