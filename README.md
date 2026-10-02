# Todo CLI

A small command-line todo application written in modern C++.

The project is used to practice C++ fundamentals including STL containers, file I/O, classes, RAII, smart pointers, move semantics, unit testing, debugging, and basic code-quality tooling.

## Features

The application supports:

- Adding tasks
- Listing tasks
- Marking tasks as completed
- Removing tasks
- Saving tasks to a local file
- Loading tasks on the next program run

Example:

```text
$ ./build/todo add Learn GoogleTest
$ ./build/todo add Practice GDB

$ ./build/todo list
1. [ ] Learn GoogleTest
2. [ ] Practice GDB

$ ./build/todo done 1

$ ./build/todo list
1. [x] Learn GoogleTest
2. [ ] Practice GDB

$ ./build/todo remove 2
```

## Project Structure

```text
todo-cli/
├── .github/
│   └── workflows/
│       └── ci.yml
├── .clang-format
├── .clang-tidy
├── CMakeLists.txt
├── README.md
├── include/
│   ├── task.h
│   └── task_manager.h
├── src/
│   ├── main.cpp
│   ├── task.cpp
│   └── task_manager.cpp
└── tests/
    ├── task_test.cpp
    └── task_manager_test.cpp
```

`Task` represents a single todo item.

`TaskManager` owns and manages the collection of tasks and handles persistence.

`main.cpp` parses command-line arguments and delegates operations to `TaskManager`.

## Requirements

- C++17-compatible compiler
- CMake 3.16 or later
- Git

Test dependencies such as GoogleTest are downloaded automatically by CMake.

## Build

Configure a Debug build:

```bash
cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug
```

Build the project:

```bash
cmake --build build
```

## Run

List tasks:

```bash
./build/todo list
```

Add a task:

```bash
./build/todo add Learn modern C++
```

Mark a task as completed:

```bash
./build/todo done 1
```

Remove a task:

```bash
./build/todo remove 1
```

## Tests

Build the project first:

```bash
cmake --build build
```

Run all tests:

```bash
ctest \
    --test-dir build \
    --output-on-failure
```

The tests cover `Task` and `TaskManager`, including normal operations, invalid task numbers, removal behavior, completion state, and save/load persistence.

## Sanitizers

A separate sanitizer build can be created if the project enables the `ENABLE_SANITIZERS` CMake option:

```bash
cmake \
    -S . \
    -B build-sanitize \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_SANITIZERS=ON

cmake --build build-sanitize

ctest \
    --test-dir build-sanitize \
    --output-on-failure
```

This build enables AddressSanitizer and UndefinedBehaviorSanitizer on supported GCC/Clang configurations.

## Code Quality

The project is compiled with warnings such as:

```text
-Wall
-Wextra
-Wpedantic
```

Source formatting is controlled by `.clang-format`.

Static analysis configuration is stored in `.clang-tidy`.

Generate a compilation database for clang-tidy with:

```bash
cmake \
    -S . \
    -B build \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Example static analysis:

```bash
clang-tidy \
    -p build \
    src/task.cpp \
    src/task_manager.cpp
```

Example formatting:

```bash
clang-format -i \
    src/*.cpp \
    include/*.h \
    tests/*.cpp
```

## Persistence

Tasks are stored in:

```text
tasks.txt
```

The file is runtime data and is normally excluded from Git.

If the file does not exist, the application starts with an empty task list.

Malformed task data is treated as an error instead of being silently ignored.

## Continuous Integration

GitHub Actions automatically builds and tests the project on pushes and pull requests.

The CI workflow is located at:

```text
.github/workflows/ci.yml
```

A change should not be considered ready if the CI build or test suite fails.

## Current Design Notes

`TaskManager` owns its tasks.

In the current learning implementation this ownership may be represented with:

```cpp
std::vector<std::unique_ptr<Task>>
```

This is primarily used to practice RAII and ownership semantics.

For a small value-type such as `Task`, a production implementation could also reasonably use:

```cpp
std::vector<Task>
```

which may be simpler and avoid unnecessary dynamic allocation.