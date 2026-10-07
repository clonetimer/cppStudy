#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <mutex>
#include <queue>
#include <utility>

template <typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue() = default;

    ThreadSafeQueue(
        const ThreadSafeQueue&
    ) = delete;

    ThreadSafeQueue& operator=(
        const ThreadSafeQueue&
    ) = delete;

    void push(T value)
    {
        std::lock_guard<std::mutex>
            lock(mutex_);

        queue_.push(
            std::move(value)
        );
    }

    bool tryPop(T& value)
    {
        std::lock_guard<std::mutex>
            lock(mutex_);

        if (queue_.empty())
        {
            return false;
        }

        value =
            std::move(
                queue_.front()
            );

        queue_.pop();

        return true;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex>
            lock(mutex_);

        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;

    std::queue<T> queue_;
};

#endif