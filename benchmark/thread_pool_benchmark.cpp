#include "thread_pool.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

std::uint64_t work(std::uint64_t iterations)
{
    std::uint64_t result = 0;

    for (std::uint64_t i = 1;
         i <= iterations;
         ++i)
    {
        result += (i * 31) ^ (i >> 3);
    }

    return result;
}

int main()
{
    const unsigned int hardwareThreads =
        std::thread::hardware_concurrency();

    const std::size_t workerCount =
        hardwareThreads == 0
            ? 4
            : hardwareThreads;

    constexpr int taskCount = 32;
    constexpr std::uint64_t iterations =
        5'000'000;

    std::uint64_t serialResult = 0;

    auto serialStart =
        std::chrono::steady_clock::now();

    for (int i = 0; i < taskCount; ++i)
    {
        serialResult += work(iterations);
    }

    auto serialEnd =
        std::chrono::steady_clock::now();

    ThreadPool pool(workerCount);

    std::vector<
        std::future<std::uint64_t>>
        futures;

    futures.reserve(taskCount);

    auto parallelStart =
        std::chrono::steady_clock::now();

    for (int i = 0; i < taskCount; ++i)
    {
        futures.push_back(
            pool.submit(
                [=]()
                {
                    return work(iterations);
                }));
    }

    std::uint64_t parallelResult = 0;

    for (auto& future : futures)
    {
        parallelResult += future.get();
    }

    auto parallelEnd =
        std::chrono::steady_clock::now();

    const auto serialMs =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            serialEnd - serialStart)
            .count();

    const auto parallelMs =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            parallelEnd - parallelStart)
            .count();

    std::cout
        << "workers: "
        << workerCount
        << '\n';

    std::cout
        << "serial: "
        << serialMs
        << " ms\n";

    std::cout
        << "thread pool: "
        << parallelMs
        << " ms\n";

    std::cout
        << "results equal: "
        << (serialResult == parallelResult)
        << '\n';

    if (parallelMs > 0)
    {
        std::cout
            << "speedup: "
            << static_cast<double>(serialMs)
                   / static_cast<double>(
                       parallelMs)
            << "x\n";
    }
}