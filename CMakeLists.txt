cmake_minimum_required(VERSION 3.18)

project(
    CppHttpServer
    VERSION 0.6.0
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
    CMAKE_CXX_COMPILER_ID
    MATCHES
    "GNU|Clang"
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
    CMAKE_CXX_COMPILER_ID
    MATCHES
    "GNU|Clang"
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
# HTTP Core
# --------------------------------------------------

add_library(
    http_core

    src/http_request_parser.cpp
    src/http_response.cpp
    src/static_file_handler.cpp
    src/router.cpp
    src/api_handlers.cpp
    src/thread_pool.cpp
    src/logger.cpp
)

target_include_directories(
    http_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    http_core
    PUBLIC
    cxx_std_17
)

target_link_libraries(
    http_core
    PUBLIC
    Threads::Threads
    project_warnings
    project_sanitizers
)

# --------------------------------------------------
# Server
# --------------------------------------------------

add_executable(
    http_server
    src/http_server.cpp
)

target_link_libraries(
    http_server
    PRIVATE
    http_core
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

    set(
        HTTP_TEST_SOURCES
    )

    foreach(
        TEST_SOURCE
        tests/http_request_parser_test.cpp
        tests/http_response_test.cpp
        tests/static_file_handler_test.cpp
        tests/router_test.cpp
    )

        if (
            EXISTS
            "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_SOURCE}"
        )

            list(
                APPEND
                HTTP_TEST_SOURCES
                ${TEST_SOURCE}
            )

        endif()

    endforeach()

    if (HTTP_TEST_SOURCES)

        add_executable(
            http_tests
            ${HTTP_TEST_SOURCES}
        )

        target_link_libraries(
            http_tests
            PRIVATE
            http_core
            GTest::gtest_main
        )

        include(GoogleTest)

        gtest_discover_tests(
            http_tests
            DISCOVERY_MODE PRE_TEST
        )

    endif()

endif()