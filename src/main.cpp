#include <chrono>
#include <iostream>
#include <thread>

void task(
    int id,
    int milliseconds
)
{
    std::cout
        << "Task "
        << id
        << " started\n";

    std::this_thread::sleep_for(
        std::chrono::milliseconds(
            milliseconds
        )
    );

    std::cout
        << "Task "
        << id
        << " finished\n";
}

int main()
{
    std::thread t1(
        task,
        1,
        1000
    );

    std::thread t2(
        task,
        2,
        500
    );

    std::thread t3(
        task,
        3,
        1500
    );

    t1.join();
    t2.join();
    t3.join();

    std::cout
        << "All tasks finished\n";
}