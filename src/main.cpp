#include "todo.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

const std::string TASK_FILE = "tasks.txt";

void printUsage()
{
    std::cout
        << "Usage:\n"
        << "  todo add <task>\n"
        << "  todo list\n"
        << "  todo done <number>\n"
        << "  todo remove <number>\n";
}

std::string joinArguments(
    int argc,
    char* argv[],
    int startIndex
)
{
    std::string result;

    for (int i = startIndex; i < argc; ++i)
    {
        if (!result.empty())
        {
            result += ' ';
        }

        result += argv[i];
    }

    return result;
}

std::size_t parseTaskNumber(
    const std::string& text
)
{
    std::size_t position = 0;

    int value = std::stoi(
        text,
        &position
    );

    if (position != text.size() ||
        value <= 0)
    {
        throw std::invalid_argument(
            "Invalid task number."
        );
    }

    return static_cast<std::size_t>(value);
}

}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printUsage();
        return 1;
    }

    const std::string command = argv[1];

    try
    {
        std::vector<Task> tasks =
            loadTasks(TASK_FILE);

        if (command == "list")
        {
            if (argc != 2)
            {
                std::cerr
                    << "Error: list takes no arguments.\n";
                return 1;
            }

            listTasks(tasks);
            return 0;
        }

        if (command == "add")
        {
            if (argc < 3)
            {
                std::cerr
                    << "Error: task text is required.\n";
                return 1;
            }

            std::string text =
                joinArguments(
                    argc,
                    argv,
                    2
                );

            tasks.push_back({
                text,
                false
            });

            saveTasks(
                TASK_FILE,
                tasks
            );

            std::cout
                << "Added: "
                << text
                << '\n';

            return 0;
        }

        if (command == "done")
        {
            if (argc != 3)
            {
                std::cerr
                    << "Error: task number is required.\n";
                return 1;
            }

            std::size_t taskNumber =
                parseTaskNumber(argv[2]);

            if (!markTaskDone(
                    tasks,
                    taskNumber))
            {
                std::cerr
                    << "Error: task "
                    << taskNumber
                    << " does not exist.\n";

                return 1;
            }

            saveTasks(
                TASK_FILE,
                tasks
            );

            std::cout
                << "Completed task "
                << taskNumber
                << ".\n";

            return 0;
        }

        if (command == "remove")
        {
            if (argc != 3)
            {
                std::cerr
                    << "Error: task number is required.\n";
                return 1;
            }

            std::size_t taskNumber =
                parseTaskNumber(argv[2]);

            if (!removeTask(
                    tasks,
                    taskNumber))
            {
                std::cerr
                    << "Error: task "
                    << taskNumber
                    << " does not exist.\n";

                return 1;
            }

            saveTasks(
                TASK_FILE,
                tasks
            );

            std::cout
                << "Removed task "
                << taskNumber
                << ".\n";

            return 0;
        }

        std::cerr
            << "Error: unknown command '"
            << command
            << "'.\n";

        printUsage();

        return 1;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Error: "
            << error.what()
            << '\n';

        return 1;
    }
}