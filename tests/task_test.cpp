#include "task.h"

#include <gtest/gtest.h>

TEST(TaskTest, StoresText)
{
    Task task("Learn GoogleTest");

    EXPECT_EQ(
        task.text(),
        "Learn GoogleTest"
    );
}

TEST(TaskTest, NewTaskIsNotDone)
{
    Task task("Learn GoogleTest");

    EXPECT_FALSE(task.isDone());
}

TEST(TaskTest, CanStartAsCompleted)
{
    Task task(
        "Existing task",
        true
    );

    EXPECT_TRUE(task.isDone());
}

TEST(TaskTest, MarkDoneChangesState)
{
    Task task("Learn GoogleTest");

    task.markDone();

    EXPECT_TRUE(task.isDone());
}

TEST(TaskTest, EqualTasksCompareEqual)
{
    Task first(
        "Learn C++",
        true
    );

    Task second(
        "Learn C++",
        true
    );

    EXPECT_TRUE(first == second);
}

TEST(TaskTest, DifferentStateIsNotEqual)
{
    Task first(
        "Learn C++",
        false
    );

    Task second(
        "Learn C++",
        true
    );

    EXPECT_FALSE(first == second);
}