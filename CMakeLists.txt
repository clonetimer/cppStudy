cmake_minimum_required(VERSION 3.16)

project(
    Calculator
    VERSION 1.0.0
    LANGUAGES CXX
)

add_library(
    calculator_lib
    src/calculator.cpp
)

target_include_directories(
    calculator_lib
    PUBLIC
    include
)

target_compile_features(
    calculator_lib
    PUBLIC
    cxx_std_17
)

add_executable(
    calculator
    src/main.cpp
)

target_link_libraries(
    calculator
    PRIVATE
    calculator_lib
)