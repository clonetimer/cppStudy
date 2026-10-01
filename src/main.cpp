#include <iostream>
#include <string>
#include <utility>

class Task
{
public:
    Task(
        std::string text,
        int priority,
        bool done = false
    )
        : text_(std::move(text)),
          priority_(priority),
          done_(done)
    {
    }

    bool operator==(const Task& other) const
    {
        return text_ == other.text_
            && priority_ == other.priority_
            && done_ == other.done_;
    }

    bool operator<(const Task& other) const
    {
        return priority_ < other.priority_;
    }

    friend std::ostream& operator<<(
        std::ostream& output,
        const Task& task
    );

private:
    std::string text_;
    int priority_;
    bool done_;
};

std::ostream& operator<<(
    std::ostream& output,
    const Task& task
)
{
    output
        << '['
        << (task.done_ ? 'x' : ' ')
        << "] "
        << task.text_
        << " (priority="
        << task.priority_
        << ')';

    return output;
}

#include <algorithm>
#include <vector>

int main()
{
    Task a("Learn C++", 2);
    Task b("Practice STL", 1);
    Task c("Learn C++", 2);

    std::cout
        << "a == c: "
        << (a == c)
        << '\n';

    std::vector<Task> tasks{
        a,
        b,
        c
    };

    std::sort(
        tasks.begin(),
        tasks.end()
    );

    for (const auto& task : tasks)
    {
        std::cout << task << '\n';
    }
}