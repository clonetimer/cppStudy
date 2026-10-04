#include "thread_pool.h"

#include <atomic>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

TEST(ThreadPoolTest, TaskReturnsValue)
{
    ThreadPool pool(2);

    auto future = pool.submit(
        []()
        {
            return 42;
        });

    EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, TaskAcceptsArguments)
{
    ThreadPool pool(2);

    auto future = pool.submit(
        [](int a, int b)
        {
            return a + b;
        },
        10,
        20);

    EXPECT_EQ(future.get(), 30);
}

TEST(ThreadPoolTest, SupportsDifferentReturnTypes)
{
    ThreadPool pool(2);

    auto number = pool.submit(
        []()
        {
            return 123;
        });

    auto text = pool.submit(
        []()
        {
            return std::string("hello");
        });

    EXPECT_EQ(number.get(), 123);
    EXPECT_EQ(text.get(), "hello");
}

TEST(ThreadPoolTest, TaskExceptionIsPropagatedThroughFuture)
{
    ThreadPool pool(2);

    auto future = pool.submit(
        []() -> int
        {
            throw std::runtime_error("boom");
        });

    EXPECT_THROW(
        future.get(),
        std::runtime_error);
}

TEST(ThreadPoolTest, FailedTaskDoesNotBreakPool)
{
    ThreadPool pool(2);

    auto bad = pool.submit(
        []() -> int
        {
            throw std::runtime_error("boom");
        });

    auto good = pool.submit(
        []()
        {
            return 42;
        });

    EXPECT_THROW(
        bad.get(),
        std::runtime_error);

    EXPECT_EQ(
        good.get(),
        42);
}

TEST(ThreadPoolTest, ShutdownDrainsExistingTasks)
{
    ThreadPool pool(4);

    std::atomic<int> counter{0};

    for (int i = 0; i < 1000; ++i)
    {
        pool.submit(
            [&counter]()
            {
                counter.fetch_add(
                    1,
                    std::memory_order_relaxed);
            });
    }

    pool.shutdown();

    EXPECT_EQ(
        counter.load(
            std::memory_order_relaxed),
        1000);
}

TEST(ThreadPoolTest, SubmitAfterShutdownThrows)
{
    ThreadPool pool(2);

    pool.shutdown();

    EXPECT_THROW(
        pool.submit(
            []()
            {
                return 42;
            }),
        std::runtime_error);
}