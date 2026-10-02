#include "task_manager.h"

#include <gtest/gtest.h>

TEST(TaskManagerTest, StartsEmpty)
{
    TaskManager manager;

    EXPECT_TRUE(manager.empty());
    EXPECT_EQ(manager.size(), 0);
}

TEST(TaskManagerTest, AddTaskIncreasesSize)
{
    TaskManager manager;

    manager.addTask("Learn GoogleTest");

    EXPECT_FALSE(manager.empty());
    EXPECT_EQ(manager.size(), 1);
}

TEST(TaskManagerTest, AddTaskStoresCorrectText)
{
    TaskManager manager;

    manager.addTask("Learn GoogleTest");

    const Task* task =
        manager.getTask(1);

    ASSERT_NE(task, nullptr);

    EXPECT_EQ(
        task->text(),
        "Learn GoogleTest"
    );

    EXPECT_FALSE(
        task->isDone()
    );
}

TEST(TaskManagerTest, MarkTaskDoneChangesTaskState)
{
    TaskManager manager;

    manager.addTask("Learn GoogleTest");

    bool result =
        manager.markTaskDone(1);

    EXPECT_TRUE(result);

    const Task* task =
        manager.getTask(1);

    ASSERT_NE(task, nullptr);

    EXPECT_TRUE(
        task->isDone()
    );
}

TEST(TaskManagerTest, MarkInvalidTaskReturnsFalse)
{
    TaskManager manager;

    manager.addTask("Learn GoogleTest");

    EXPECT_FALSE(
        manager.markTaskDone(0)
    );

    EXPECT_FALSE(
        manager.markTaskDone(2)
    );
}

TEST(TaskManagerTest, RemoveTaskDecreasesSize)
{
    TaskManager manager;

    manager.addTask("Task A");
    manager.addTask("Task B");

    bool result =
        manager.removeTask(1);

    EXPECT_TRUE(result);
    EXPECT_EQ(manager.size(), 1);
}

TEST(TaskManagerTest, RemoveTaskRemovesCorrectTask)
{
    TaskManager manager;

    manager.addTask("Task A");
    manager.addTask("Task B");

    manager.removeTask(1);

    ASSERT_EQ(manager.size(), 1);

    const Task* remaining =
        manager.getTask(1);

    ASSERT_NE(remaining, nullptr);

    EXPECT_EQ(
        remaining->text(),
        "Task B"
    );
}

TEST(TaskManagerTest, RemoveInvalidTaskReturnsFalse)
{
    TaskManager manager;

    manager.addTask("Task A");

    EXPECT_FALSE(
        manager.removeTask(0)
    );

    EXPECT_FALSE(
        manager.removeTask(2)
    );

    EXPECT_EQ(
        manager.size(),
        1
    );
}

#include <filesystem>
TEST(TaskManagerTest, SaveAndLoadPreservesTasks)
{
    const std::string filename =
        "test_tasks.txt";

    {
        TaskManager manager;

        manager.addTask("Task A");
        manager.addTask("Task B");

        manager.markTaskDone(2);

        manager.save(filename);
    }

    {
        TaskManager loaded;

        loaded.load(filename);

        ASSERT_EQ(
            loaded.size(),
            2
        );

        const Task* first =
            loaded.getTask(1);

        const Task* second =
            loaded.getTask(2);

        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);

        EXPECT_EQ(
            first->text(),
            "Task A"
        );

        EXPECT_FALSE(
            first->isDone()
        );

        EXPECT_EQ(
            second->text(),
            "Task B"
        );

        EXPECT_TRUE(
            second->isDone()
        );
    }

    std::filesystem::remove(filename);
}