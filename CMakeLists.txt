cmake_minimum_required(VERSION 3.18)

project(
    HttpServer
    VERSION 0.1.0
    LANGUAGES CXX
)

add_executable(
    http_server

    src/http_server.cpp
    src/http_request_parser.cpp
    src/http_response.cpp
    src/static_file_handler.cpp
    src/router.cpp
    src/api_handlers.cpp
)

target_include_directories(
    http_server
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(
    http_server
    PRIVATE
    cxx_std_17
)

if (
    CMAKE_CXX_COMPILER_ID
    MATCHES "GNU|Clang"
)
    target_compile_options(
        http_server
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()