cmake_minimum_required(VERSION 3.16)

project(
    TcpChat
    VERSION 1.0.0
    LANGUAGES CXX
)

find_package(
    Threads
    REQUIRED
)

add_executable(
    chat_server
    src/server.cpp
)

add_executable(
    chat_client
    src/client.cpp
)

target_include_directories(
    chat_server
    PRIVATE
    include
)

target_include_directories(
    chat_client
    PRIVATE
    include
)

target_compile_features(
    chat_server
    PRIVATE
    cxx_std_17
)

target_compile_features(
    chat_client
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    chat_client
    PRIVATE
    Threads::Threads
)

if (
    CMAKE_CXX_COMPILER_ID
    MATCHES "GNU|Clang"
)
    target_compile_options(
        chat_server
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_compile_options(
        chat_client
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()