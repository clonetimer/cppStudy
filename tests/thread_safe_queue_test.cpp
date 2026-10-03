#include "thread_safe_queue.h"

#include <algorithm>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

TEST(ThreadSafeQueueTest, PushThenPopReturnsValue)
{
    ThreadSafeQueue<int> queue;

    ASSERT_TRUE(queue.push(42));

    auto value = queue.waitAndPop();

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 42);
}

TEST(ThreadSafeQueueTest, ClosedEmptyQueueReturnsNullopt)
{
    ThreadSafeQueue<int> queue;

    queue.close();

    auto value = queue.waitAndPop();

    EXPECT_FALSE(value.has_value());
}

TEST(ThreadSafeQueueTest, PushAfterCloseFails)
{
    ThreadSafeQueue<int> queue;

    queue.close();

    EXPECT_FALSE(queue.push(42));
}

TEST(ThreadSafeQueueTest, CloseStillAllowsQueuedItemsToDrain)
{
    ThreadSafeQueue<int> queue;

    ASSERT_TRUE(queue.push(10));
    ASSERT_TRUE(queue.push(20));

    queue.close();

    auto first = queue.waitAndPop();
    auto second = queue.waitAndPop();
    auto third = queue.waitAndPop();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());

    EXPECT_EQ(*first, 10);
    EXPECT_EQ(*second, 20);

    EXPECT_FALSE(third.has_value());
}

TEST(ThreadSafeQueueTest, ConsumerReceivesItemProducedLater)
{
    ThreadSafeQueue<int> queue;

    int result = 0;

    std::thread consumer(
        [&]()
        {
            auto value = queue.waitAndPop();

            ASSERT_TRUE(value.has_value());

            result = *value;
        });

    ASSERT_TRUE(queue.push(123));

    consumer.join();

    EXPECT_EQ(result, 123);
}