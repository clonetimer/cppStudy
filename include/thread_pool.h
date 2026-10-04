#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

class ThreadPool
{
public:
    explicit ThreadPool(
        std::size_t workerCount
    )
    {
        if (workerCount == 0)
        {
            throw std::invalid_argument(
                "workerCount must be greater than zero"
            );
        }

        for (std::size_t i = 0;
             i < workerCount;
             ++i)
        {
            workers_.emplace_back(
                [this]()
                {
                    workerLoop();
                }
            );
        }
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            stopping_ = true;
        }

        condition_.notify_all();

        for (auto& worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(
        std::function<void()> task
    )
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (stopping_)
            {
                throw std::runtime_error(
                    "cannot submit task to stopped ThreadPool"
                );
            }

            tasks_.push(
                std::move(task)
            );
        }

        condition_.notify_one();
    }

private:
    void workerLoop()
    {
        while (true)
        {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                condition_.wait(
                    lock,
                    [this]()
                    {
                        return stopping_
                            || !tasks_.empty();
                    }
                );

                if (stopping_ &&
                    tasks_.empty())
                {
                    return;
                }

                task =
                    std::move(
                        tasks_.front()
                    );

                tasks_.pop();
            }

            task();
        }
    }

private:
    std::vector<std::thread> workers_;

    std::queue<
        std::function<void()>
    > tasks_;

    std::mutex mutex_;

    std::condition_variable condition_;

    bool stopping_ = false;
};

#endif