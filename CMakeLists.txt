cmake_minimum_required(VERSION 3.18)

project(
    TcpChat
    VERSION 1.0.0
    LANGUAGES CXX
)

set(
    CMAKE_EXPORT_COMPILE_COMMANDS
    ON
)

include(CTest)

find_package(
    Threads
    REQUIRED
)

option(
    ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

# --------------------------------------------------
# Warnings
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
    ENABLE_SANITIZERS
    AND
    CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
)
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

# --------------------------------------------------
# Protocol library
# --------------------------------------------------

add_library(
    chat_core
    src/chat_protocol.cpp
)

target_include_directories(
    chat_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    chat_core
    PUBLIC
    cxx_std_17
)

target_link_libraries(
    chat_core
    PUBLIC
    project_warnings
    project_sanitizers
)

# --------------------------------------------------
# Server
# --------------------------------------------------

add_executable(
    chat_server
    src/server.cpp
)

target_link_libraries(
    chat_server
    PRIVATE
    chat_core
)

# --------------------------------------------------
# Client
# --------------------------------------------------

add_executable(
    chat_client
    src/client.cpp
)

target_link_libraries(
    chat_client
    PRIVATE
    chat_core
    Threads::Threads
)

# --------------------------------------------------
# Tests
# --------------------------------------------------

if (BUILD_TESTING)

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

    set(
        INSTALL_GTEST
        OFF
        CACHE BOOL ""
        FORCE
    )

    FetchContent_MakeAvailable(
        googletest
    )

    add_executable(
        chat_tests
        tests/chat_protocol_test.cpp
    )

    target_link_libraries(
        chat_tests
        PRIVATE
        chat_core
        GTest::gtest_main
    )

    include(GoogleTest)

    gtest_discover_tests(
        chat_tests
        DISCOVERY_MODE PRE_TEST
    )

endif()