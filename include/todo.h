#ifndef TODO_H
#define TODO_H

#include <cstddef>
#include <string>
#include <vector>

struct Task
{
    std::string text;
    bool done = false;
};

std::vector<Task> loadTasks(const std::string& filename);

void saveTasks(
    const std::string& filename,
    const std::vector<Task>& tasks
);

void listTasks(const std::vector<Task>& tasks);

bool markTaskDone(
    std::vector<Task>& tasks,
    std::size_t taskNumber
);

bool removeTask(
    std::vector<Task>& tasks,
    std::size_t taskNumber
);

#endif