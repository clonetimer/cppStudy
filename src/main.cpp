#include "thread_pool.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

std::mutex outputMutex;

void log(
    const std::string& message
)
{
    std::lock_guard<std::mutex> lock(
        outputMutex
    );

    std::cout
        << message
        << '\n';
}

int main()
{
    ThreadPool pool(4);

    for (int i = 0; i < 8; ++i)
    {
        pool.submit(
            [i]()
            {
                log(
                    "Task "
                    + std::to_string(i)
                    + " start"
                );

                std::this_thread::sleep_for(
                    std::chrono::seconds(1)
                );

                log(
                    "Task "
                    + std::to_string(i)
                    + " end"
                );
            }
        );
    }
}