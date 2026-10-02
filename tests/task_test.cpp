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

TEST(TaskTest, MarkDoneChangesState)
{
    Task task("Learn GoogleTest");

    task.markDone();

    EXPECT_TRUE(task.isDone());
}

TEST(TaskTest, CanRestoreCompletedTask)
{
    Task task(
        "Existing task",
        true
    );

    EXPECT_TRUE(task.isDone());
}