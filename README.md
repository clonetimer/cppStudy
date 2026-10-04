# C++17 Thread Pool

A small but practical C++17 thread pool implemented from scratch for learning modern C++ concurrency and resource-management techniques.

The project demonstrates:

- Worker-thread reuse
- Thread-safe task queuing
- `std::mutex`
- `std::condition_variable`
- RAII-based thread lifetime management
- `std::future`
- `std::packaged_task`
- Perfect forwarding
- Exception propagation
- Graceful shutdown
- Atomic statistics
- Unit testing
- Sanitizers
- Basic benchmarking
- Thread-safe logging

## Architecture

The core execution model is:

```text
                submit()
                   |
                   v
        +----------------------+
        |      Task Queue      |
        | std::function<void()>|
        +----------------------+
                   |
         mutex + condition
                   |
        +----------+----------+
        |          |          |
        v          v          v
     Worker 1   Worker 2   Worker N
        |          |          |
        +------ execute -------+
```

Worker threads are created when the `ThreadPool` is constructed and are reused for multiple tasks.

Workers sleep on a `std::condition_variable` when no work is available instead of busy waiting.

## Task Submission

Tasks may return values:

```cpp
ThreadPool pool(4);

auto result = pool.submit(
    [](int a, int b)
    {
        return a + b;
    },
    10,
    20
);

std::cout << result.get() << '\n';
```

Output:

```text
30
```

`submit()` automatically returns:

```cpp
std::future<ReturnType>
```

where `ReturnType` is inferred from the submitted callable.

## Exception Propagation

Exceptions thrown by tasks are propagated through their futures:

```cpp
auto future = pool.submit(
    []() -> int
    {
        throw std::runtime_error("task failed");
    }
);

try
{
    future.get();
}
catch (const std::exception& error)
{
    std::cerr << error.what() << '\n';
}
```

A failed task does not terminate the entire thread pool.

## Shutdown

The pool performs graceful shutdown.

The lifecycle is:

```text
Running
   |
   | shutdown()
   v
Stopping
   |
   | reject new submissions
   | execute already queued tasks
   v
Queue empty
   |
   v
Workers exit
   |
   v
Joined / stopped
```

Once shutdown begins:

- New tasks are rejected.
- Already submitted tasks continue running.
- Workers exit only when the queue becomes empty.
- All worker threads are joined.

The destructor automatically performs shutdown, following RAII principles.

## Requirements

- C++17-compatible compiler
- CMake 3.16 or later
- POSIX threads / supported C++ threading implementation
- Git

GoogleTest is downloaded automatically when tests are enabled.

## Build

Debug build:

```bash
cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel
```

Run the demo:

```bash
./build/thread_pool_demo
```

## Tests

Run:

```bash
ctest \
    --test-dir build \
    --output-on-failure
```

The test suite covers areas including:

- Returning task results
- Argument forwarding
- Multiple return types
- Future-based exception propagation
- Worker survival after task failure
- Graceful shutdown
- Rejecting submissions after shutdown
- Concurrent execution

## AddressSanitizer / UndefinedBehaviorSanitizer

Create a separate sanitizer build:

```bash
cmake \
    -S . \
    -B build-sanitize \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_ASAN_UBSAN=ON

cmake --build build-sanitize --parallel

ctest \
    --test-dir build-sanitize \
    --output-on-failure
```

## ThreadSanitizer

ThreadSanitizer is built separately from ASan/UBSan:

```bash
cmake \
    -S . \
    -B build-tsan \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_TSAN=ON

cmake --build build-tsan --parallel

ctest \
    --test-dir build-tsan \
    --output-on-failure
```

TSan is used to detect data races in concurrent code.

Some Linux or WSL configurations may have ThreadSanitizer runtime/address-space compatibility issues unrelated to the thread-pool implementation.

## Benchmark

The included benchmark compares serial execution with the thread pool.

Always use a Release build for performance measurements:

```bash
cmake \
    -S . \
    -B build-release \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF

cmake --build build-release --parallel

./build-release/thread_pool_benchmark
```

The benchmark validates that serial and parallel executions produce the same result before reporting timing information.

Benchmark results are intended for rough trend comparison rather than rigorous microbenchmarking.

Do not benchmark with sanitizers or verbose logging enabled.

## Logging

The project includes a small thread-safe logger for observing events such as:

- Worker startup
- Worker shutdown
- Pool shutdown
- Task lifecycle information

Logging is synchronized to avoid interleaved output from multiple threads.

Verbose logging should be disabled during performance measurements.

## Project Structure

```text
thread-pool/
├── .github/
│   └── workflows/
│       └── ci.yml
├── benchmark/
│   └── thread_pool_benchmark.cpp
├── include/
│   ├── logger.h
│   └── thread_pool.h
├── src/
│   ├── main.cpp
│   └── thread_pool.cpp
├── tests/
│   └── thread_pool_test.cpp
├── .clang-format
├── .clang-tidy
├── .gitignore
├── CMakeLists.txt
└── README.md
```

## Design Notes

The queue stores:

```cpp
std::function<void()>
```

Submitted functions and their arguments are wrapped in `std::packaged_task`, allowing task return values and exceptions to be transferred through `std::future`.

In the C++17 implementation, a `std::shared_ptr<std::packaged_task<...>>` is used to adapt the move-only packaged task to the copyable callable requirements of `std::function`.

Shared queue state is protected with a mutex.

Simple independent statistics may use `std::atomic`.

Atomic variables are not used as a replacement for mutexes when multiple pieces of state must remain consistent.

## Current Limitations

This is intentionally a compact educational thread pool.

It currently does not implement:

- Task priorities
- Work stealing
- Dynamic worker resizing
- Task cancellation
- Bounded queues / backpressure
- CPU affinity
- Coroutine integration
- Dependency graphs
- Production-grade scheduling policies

These features are intentionally outside the scope of the first stable release.