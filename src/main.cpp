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
    ThreadPool pool(4);

    auto sum =
        pool.submit(
            add,
            10,
            20
        );

    auto text =
        pool.submit(
            [](std::string value)
            {
                return value
                    + " ThreadPool";
            },
            std::string("Hello")
        );

    std::vector<
        std::future<int>
    > futures;

    for (int i = 0; i < 10; ++i)
    {
        futures.push_back(
            pool.submit(
                [i]()
                {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(100)
                    );

                    return i * i;
                }
            )
        );
    }

    std::cout
        << "sum = "
        << sum.get()
        << '\n';

    std::cout
        << text.get()
        << '\n';

    for (auto& future : futures)
    {
        std::cout
            << future.get()
            << ' ';
    }

    std::cout << '\n';
}