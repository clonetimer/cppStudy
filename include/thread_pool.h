#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool
{
public:
    using Task =
        std::function<void()>;

    explicit ThreadPool(
        std::size_t threadCount,
        std::size_t maxPendingTasks = 1024
    );

    ~ThreadPool();

    ThreadPool(
        const ThreadPool&
    ) = delete;

    ThreadPool& operator=(
        const ThreadPool&
    ) = delete;

    /*
     * 成功加入队列：
     * true
     *
     * queue 满 / pool stopping：
     * false
     */
    bool submit(Task task);

    /*
     * 停止接收新任务，
     * 等待现有任务完成，
     * join 所有 Worker。
     */
    void shutdown();

private:
    void workerLoop();

    std::vector<std::thread>
        workers_;

    std::queue<Task>
        tasks_;

    std::mutex mutex_;

    std::condition_variable
        condition_;

    std::size_t
        maxPendingTasks_;

    bool stopping_ = false;
};

#endif