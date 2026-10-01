#ifndef TASK_H
#define TASK_H

#include <iosfwd>
#include <string>

class Task
{
public:
    Task(
        std::string text,
        bool done = false
    );

    const std::string& text() const;

    bool isDone() const;

    void markDone();

    bool operator==(const Task& other) const;

    friend std::ostream& operator<<(
        std::ostream& output,
        const Task& task
    );

private:
    std::string text_;
    bool done_;
};

#endif