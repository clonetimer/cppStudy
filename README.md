# Command Line Calculator

A simple command-line calculator written in C++.

The project supports basic arithmetic operations and is organized using separate source and header files. CMake is used as the build system.

## Features

- Addition: `+`
- Subtraction: `-`
- Multiplication: `*`
- Division: `/`
- Continuous calculations without restarting the program
- Exit using `q` or `Q`
- Invalid input handling
- Unsupported operator handling
- Division-by-zero handling

## Example

```text
=== Calculator ===

Enter first number (or q to quit): 10
Enter operator (+ - * /): +
Enter second number: 20
Result: 30

Enter first number (or q to quit): 8
Enter operator (+ - * /): /
Enter second number: 2
Result: 4

Enter first number (or q to quit): q
Bye.
```

## Project Structure

```text
calculator/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── include/
│   └── calculator.h
└── src/
    ├── calculator.cpp
    └── main.cpp
```

`calculator.h` declares the calculator functions.

`calculator.cpp` implements the arithmetic operations.

`main.cpp` handles user input, program flow, and error reporting.

## Requirements

- C++17 compatible compiler
- CMake 3.16 or later

Example tools:

- GCC / G++
- CMake

## Build

From the project root directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The executable will be generated in the `build` directory.

## Run

```bash
./build/calculator
```

Example:

```text
Enter first number: 10
Enter operator (+ - * /): *
Enter second number: 5
Result: 50
```

## Error Handling

Invalid number:

```text
Enter first number: abc
Error: first input must be a number.
```

Unsupported operator:

```text
Enter first number: 10
Enter operator (+ - * /): %
Enter second number: 2
Error: unsupported operator '%'.
```

Division by zero:

```text
Enter first number: 10
Enter operator (+ - * /): /
Enter second number: 0
Error: division by zero
```

## Build Design

The calculation logic is built as a separate CMake library target:

```text
calculator.cpp
      ↓
calculator_lib
      ↓
     link
      ↓
calculator executable
      ↑
   main.cpp
```

This separates calculation logic from command-line input and output.

## Current Limitations

- Only one calculation is performed per program execution.
- Supported operators are limited to `+`, `-`, `*`, and `/`.
- The program does not currently retry after invalid input.