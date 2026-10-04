cmake_minimum_required(VERSION 3.16)

project(
    ProducerConsumer
    VERSION 1.0.0
    LANGUAGES CXX
)

find_package(
    Threads
    REQUIRED
)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL
    https://github.com/google/googletest/archive/refs/tags/v1.18.0.zip
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
    producer_consumer
    src/main.cpp
)

target_include_directories(
    producer_consumer
    PRIVATE
    include
)

target_compile_features(
    producer_consumer
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    producer_consumer
    PRIVATE
    Threads::Threads
)

add_executable(
    queue_tests
    tests/thread_safe_queue_test.cpp
)

target_include_directories(
    queue_tests
    PRIVATE
    include
)

target_compile_features(
    queue_tests
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    queue_tests
    PRIVATE
    Threads::Threads
    GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(
    queue_tests
    DISCOVERY_MODE PRE_TEST
)

option(
    ENABLE_TSAN
    "Enable ThreadSanitizer"
    OFF
)

if (
    ENABLE_TSAN
    AND
    CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
)
    target_compile_options(
        producer_consumer
        PRIVATE
        -fsanitize=thread
        -fno-omit-frame-pointer
    )

    target_link_options(
        producer_consumer
        PRIVATE
        -fsanitize=thread
    )

    target_compile_options(
        queue_tests
        PRIVATE
        -fsanitize=thread
        -fno-omit-frame-pointer
    )

    target_link_options(
        queue_tests
        PRIVATE
        -fsanitize=thread
    )

add_executable(
    thread_pool_benchmark
    benchmark/thread_pool_benchmark.cpp
)

target_link_libraries(
    thread_pool_benchmark
    PRIVATE
    thread_pool
    Threads::Threads
)

endif()