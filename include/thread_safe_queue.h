#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

template <typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue() = default;

    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    bool push(T value)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (closed_)
            {
                return false;
            }

            queue_.push(std::move(value));
        }

        condition_.notify_one();

        return true;
    }

    std::optional<T> waitAndPop()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        condition_.wait(
            lock,
            [this]()
            {
                return !queue_.empty() || closed_;
            }
        );

        if (queue_.empty())
        {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop();

        return value;
    }

    void close()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            closed_ = true;
        }

        condition_.notify_all();
    }

    bool isClosed() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        return closed_;
    }

private:
    std::queue<T> queue_;

    mutable std::mutex mutex_;

    std::condition_variable condition_;

    bool closed_ = false;
};

#endif