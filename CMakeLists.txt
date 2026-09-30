cmake_minimum_required(VERSION 3.16)

project(
    TodoCLI
    VERSION 1.0.0
    LANGUAGES CXX
)

add_library(
    todo_lib
    src/todo.cpp
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

add_executable(
    todo
    src/main.cpp
)

target_link_libraries(
    todo
    PRIVATE
    todo_lib
)