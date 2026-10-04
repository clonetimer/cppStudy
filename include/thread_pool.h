#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool
{
public:
    explicit ThreadPool(
        std::size_t workerCount
    );

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(
        const ThreadPool&
    ) = delete;

    template <typename F, typename... Args>
    auto submit(
        F&& function,
        Args&&... args
    )
        -> std::future<
            std::invoke_result_t<
                F,
                Args...
            >
        >
    {
        using ReturnType =
            std::invoke_result_t<
                F,
                Args...
            >;

        auto task =
            std::make_shared<
                std::packaged_task<
                    ReturnType()
                >
            >(
                std::bind(
                    std::forward<F>(
                        function
                    ),
                    std::forward<Args>(
                        args
                    )...
                )
            );

        std::future<ReturnType> future =
            task->get_future();

        {
            std::lock_guard<std::mutex> lock(
                mutex_
            );

            if (stopping_)
            {
                throw std::runtime_error(
                    "cannot submit task to stopped ThreadPool"
                );
            }

            tasks_.push(
                [task]()
                {
                    (*task)();
                }
            );
        }

        condition_.notify_one();

        return future;
    }

private:
    void workerLoop();

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