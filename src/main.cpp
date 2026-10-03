#include "thread_safe_queue.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

std::mutex outputMutex;

void printLine(const std::string& text)
{
    std::lock_guard<std::mutex> lock(outputMutex);

    std::cout << text << '\n';
}

void producer(
    ThreadSafeQueue<int>& queue,
    int producerId,
    int start,
    int count
)
{
    for (int i = 0; i < count; ++i)
    {
        int value = start + i;

        if (!queue.push(value))
        {
            return;
        }

        printLine(
            "Producer "
            + std::to_string(producerId)
            + " produced "
            + std::to_string(value)
        );

        std::this_thread::sleep_for(
            std::chrono::milliseconds(50)
        );
    }
}

void consumer(
    ThreadSafeQueue<int>& queue,
    int consumerId
)
{
    while (true)
    {
        std::optional<int> value =
            queue.waitAndPop();

        if (!value)
        {
            break;
        }

        printLine(
            "Consumer "
            + std::to_string(consumerId)
            + " processed "
            + std::to_string(*value)
        );

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100)
        );
    }

    printLine(
        "Consumer "
        + std::to_string(consumerId)
        + " exiting"
    );
}

int main()
{
    ThreadSafeQueue<int> queue;

    std::thread producer1(
        producer,
        std::ref(queue),
        1,
        100,
        5
    );

    std::thread producer2(
        producer,
        std::ref(queue),
        2,
        200,
        5
    );

    std::thread consumer1(
        consumer,
        std::ref(queue),
        1
    );

    std::thread consumer2(
        consumer,
        std::ref(queue),
        2
    );

    producer1.join();
    producer2.join();

    queue.close();

    consumer1.join();
    consumer2.join();

    std::cout << "All work finished.\n";

    return 0;
}