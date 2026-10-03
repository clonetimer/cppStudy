#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

std::vector<int> values;
std::mutex valuesMutex;

void worker(int start)
{
    for (int i = 0; i < 10000; ++i)
    {
        std::lock_guard<std::mutex> lock(
            valuesMutex
        );

        values.push_back(start + i);
    }
}

int main()
{
    std::thread t1(worker, 0);
    std::thread t2(worker, 10000);

    t1.join();
    t2.join();

    std::cout
        << "size = "
        << values.size()
        << '\n';
}