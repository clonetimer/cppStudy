#include "task_manager.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>

const Task* TaskManager::getTask(
    std::size_t taskNumber
) const
{
    if (taskNumber == 0 || 
        taskNumber > tasks_.size())
    {
        return nullptr;
    }

    return tasks_[taskNumber - 1].get();
}

void TaskManager::addTask(std::string text)
{
    tasks_.push_back(
        std::make_unique<Task>(
            std::move(text)
        )
    );
}

bool TaskManager::markTaskDone(
    std::size_t taskNumber
)
{
    if (taskNumber == 0 ||
        taskNumber > tasks_.size())
    {
        return false;
    }

    tasks_[taskNumber - 1]->markDone();

    return true;
}

bool TaskManager::removeTask(
    std::size_t taskNumber
)
{
    if (taskNumber == 0 ||
        taskNumber > tasks_.size())
    {
        return false;
    }

    auto offset =
        static_cast<
            std::vector<
                std::unique_ptr<Task>
            >::difference_type
        >(taskNumber - 1);

    tasks_.erase(
        tasks_.begin() + offset
    );

    return true;
}

void TaskManager::listTasks() const
{
    if (tasks_.empty())
    {
        std::cout << "No tasks.\n";
        return;
    }

    for (std::size_t i = 0;
         i < tasks_.size();
         ++i)
    {
        std::cout
            << i + 1
            << ". "
            << *tasks_[i]
            << '\n';
    }
}

std::size_t TaskManager::size() const
{
    return tasks_.size();
}

bool TaskManager::empty() const
{
    return tasks_.empty();
}

void TaskManager::load(
    const std::string& filename
)
{
    tasks_.clear();

    if (!std::filesystem::exists(filename))
    {
        return;
    }

    std::ifstream input(filename);

    if (!input)
    {
        throw std::runtime_error(
            "Failed to open task file."
        );
    }

    int doneValue = 0;
    std::string text;

    while (
        input
        >> doneValue
        >> std::quoted(text)
    )
    {
        if (doneValue != 0 &&
            doneValue != 1)
        {
            throw std::runtime_error(
                "Invalid task status."
            );
        }

        tasks_.push_back(
            std::make_unique<Task>(
                text,
                doneValue == 1
            )
        );
    }

    if (!input.eof())
    {
        throw std::runtime_error(
            "Invalid task file format."
        );
    }
}

void TaskManager::save(
    const std::string& filename
) const
{
    std::ofstream output(filename);

    if (!output)
    {
        throw std::runtime_error(
            "Failed to open task file for writing."
        );
    }

    for (const auto& task : tasks_)
    {
        output
            << (task->isDone() ? 1 : 0)
            << ' '
            << std::quoted(task->text())
            << '\n';
    }

    if (!output)
    {
        throw std::runtime_error(
            "Failed while writing task file."
        );
    }
}