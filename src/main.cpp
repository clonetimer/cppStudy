#include "thread_pool.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    ThreadPool pool(4);

    auto first =
        pool.submit(
            []()
            {
                return 10 + 20;
            }
        );

    auto second =
        pool.submit(
            []()
            {
                return std::string(
                    "Hello ThreadPool"
                );
            }
        );

    auto third =
        pool.submit(
            []()
            {
                std::this_thread::sleep_for(
                    std::chrono::seconds(1)
                );

                return 100;
            }
        );
    
    std::cout
        << first.get()
        << '\n';

    std::cout
        << second.get()
        << '\n';

    std::cout
        << third.get()
        << '\n';

    auto bad =
        pool.submit(
            []() -> int
            {
                throw std::runtime_error(
                    "boom"
                );
            }
        );
    
    try
    {
        bad.get();
    }
    catch (const std::exception& error)
    {
        std::cout
            << "Caught: "
            << error.what()
            << '\n';
    }    
}