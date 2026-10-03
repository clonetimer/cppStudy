#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <iostream>

class BlockingQueue
{
public:
    void push(std::string value)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            queue_.push(value);
        }

        condition_.notify_one();
    }

    std::string pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        condition_.wait(
            lock,
            [this]()
            {
                return !queue_.empty();
            }
        );

        std::string value = queue_.front();
        queue_.pop();

        return value;
    }

private:
    std::queue<std::string> queue_;

    std::mutex mutex_;

    std::condition_variable condition_;
};

BlockingQueue queue;

void producer()
{
    queue.push("load file");
    queue.push("process data");
    queue.push("save results");
}

void consumer()
{
    std::string value = queue.pop();

    std::cout << value << '\n';
}

int main()
{
    std::thread producerThread(producer);

    std::thread c1(consumer);
    std::thread c2(consumer);
    std::thread c3(consumer);

    producerThread.join();

    c1.join();
    c2.join();
    c3.join();
}