#include "thread_pool.h"

#include <stdexcept>
#include <utility>

ThreadPool::ThreadPool(
    std::size_t threadCount,
    std::size_t maxPendingTasks
)
    : maxPendingTasks_(
        maxPendingTasks
    )
{
    if (threadCount == 0)
    {
        threadCount = 1;
    }

    if (maxPendingTasks_ == 0)
    {
        throw std::invalid_argument(
            "maxPendingTasks must be greater than zero"
        );
    }

    try
    {
        workers_.reserve(
            threadCount
        );

        for (
            std::size_t i = 0;
            i < threadCount;
            ++i
        )
        {
            workers_.emplace_back(
                &ThreadPool::workerLoop,
                this
            );
        }
    }
    catch (...)
    {
        {
            std::lock_guard<std::mutex>
                lock(mutex_);

            stopping_ = true;
        }

        condition_.notify_all();

        for (
            auto& worker :
            workers_
        )
        {
            if (
                worker.joinable()
            )
            {
                worker.join();
            }
        }

        throw;
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

bool ThreadPool::submit(
    Task task
)
{
    {
        std::lock_guard<std::mutex>
            lock(mutex_);

        if (stopping_)
        {
            return false;
        }

        if (
            tasks_.size()
            >= maxPendingTasks_
        )
        {
            return false;
        }

        tasks_.push(
            std::move(task)
        );
    }

    condition_.notify_one();

    return true;
}

void ThreadPool::shutdown()
{
    {
        std::lock_guard<std::mutex>
            lock(mutex_);

        if (stopping_)
        {
            /*
             * 仍然继续下面的 join。
             */
        }
        else
        {
            stopping_ = true;
        }
    }

    condition_.notify_all();

    for (
        auto& worker :
        workers_
    )
    {
        if (
            worker.joinable()
        )
        {
            worker.join();
        }
    }

    workers_.clear();
}

void ThreadPool::workerLoop()
{
    while (true)
    {
        Task task;

        {
            std::unique_lock<std::mutex>
                lock(mutex_);

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
                &&
                tasks_.empty()
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

        /*
         * 即使某个 task 忘了自己 catch，
         * 也不要让整个 worker thread 消失。
         */
        try
        {
            task();
        }
        catch (...)
        {
            /*
             * HTTP worker 自己还会负责
             * 将业务异常转换成 500。
             *
             * 这里属于最终保护层。
             */
        }
    }
}