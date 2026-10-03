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

if (
    CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
)
    target_compile_options(
        producer_consumer
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()