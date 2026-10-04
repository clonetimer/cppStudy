#include "thread_pool.h"

ThreadPool::ThreadPool(
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

ThreadPool::~ThreadPool()
{
    shutdown();
}

void ThreadPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (stopping_)
        {
            return;
        }

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

bool ThreadPool::isStopping() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return stopping_;
}

void ThreadPool::workerLoop()
{
    while (true)
    {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(
                mutex_
            );

            condition_.wait(
                lock,
                [this]()
                {
                    return stopping_
                        || !tasks_.empty();
                }
            );

            if (
                stopping_
                && tasks_.empty()
            )
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