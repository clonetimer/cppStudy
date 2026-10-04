#include "thread_pool.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int add(int a, int b)
{
    return a + b;
}

int main()
{
    ThreadPool pool(2);

    auto bad =
        pool.submit(
            []() -> int
            {
                throw std::runtime_error(
                    "boom"
                );
            }
        );

    auto good =
        pool.submit(
            []()
            {
                return 42;
            }
        );

    try
    {
        bad.get();
    }
    catch (const std::exception& error)
    {
        std::cout
            << "bad task: "
            << error.what()
            << '\n';
    }

    std::cout
        << "good task: "
        << good.get()
        << '\n';

}