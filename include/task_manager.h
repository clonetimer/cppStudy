#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "task.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class TaskManager
{
public:
    void addTask(std::string text);

    bool markTaskDone(std::size_t taskNumber);

    bool removeTask(std::size_t taskNumber);

    void listTasks() const;

    std::size_t size() const;

    bool empty() const;

    void load(const std::string& filename);

    void save(const std::string& filename) const;

private:
    std::vector<std::unique_ptr<Task>> tasks_;
};

#endif