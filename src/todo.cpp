#include "todo.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

std::vector<Task> loadTasks(const std::string& filename)
{
    std::vector<Task> tasks;

    if (!std::filesystem::exists(filename))
    {
        return tasks;
    }

    std::ifstream input(filename);

    if (!input)
    {
        throw std::runtime_error(
            "Failed to open task file."
        );
    }

    bool done = false;
    std::string text;

    while (input >> done >> std::quoted(text))
    {
        tasks.push_back({
            text,
            done
        });
    }

    if (!input.eof())
    {
        throw std::runtime_error(
            "Invalid task file format."
        );
    }

    return tasks;
}

void saveTasks(
    const std::string& filename,
    const std::vector<Task>& tasks
)
{
    std::ofstream output(filename);

    if (!output)
    {
        throw std::runtime_error(
            "Failed to save task file."
        );
    }

    for (const auto& task : tasks)
    {
        output
            << task.done
            << ' '
            << std::quoted(task.text)
            << '\n';
    }

    if (!output)
    {
        throw std::runtime_error(
            "Failed while writing task file."
        );
    }
}

void listTasks(const std::vector<Task>& tasks)
{
    if (tasks.empty())
    {
        std::cout << "No tasks.\n";
        return;
    }

    for (std::size_t i = 0; i < tasks.size(); ++i)
    {
        const Task& task = tasks[i];

        std::cout
            << i + 1
            << ". ["
            << (task.done ? 'x' : ' ')
            << "] "
            << task.text
            << '\n';
    }
}

bool markTaskDone(
    std::vector<Task>& tasks,
    std::size_t taskNumber
)
{
    if (taskNumber == 0 ||
        taskNumber > tasks.size())
    {
        return false;
    }

    tasks[taskNumber - 1].done = true;

    return true;
}

bool removeTask(
    std::vector<Task>& tasks,
    std::size_t taskNumber
)
{
    if (taskNumber == 0 ||
        taskNumber > tasks.size())
    {
        return false;
    }

    auto offset =
        static_cast<std::vector<Task>::difference_type>(
            taskNumber - 1
        );

    tasks.erase(
        tasks.begin() + offset
    );

    return true;
}