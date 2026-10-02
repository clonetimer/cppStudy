#include "task_manager.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

const std::string TASK_FILE =
    "tasks.txt";

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

    for (int i = startIndex;
         i < argc;
         ++i)
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

    int value =
        std::stoi(
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

    return static_cast<std::size_t>(
        value
    );
}

}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printUsage();
        return 1;
    }

    try
    {
        TaskManager manager;

        manager.load(TASK_FILE);

        const std::string command =
            argv[1];

        if (command == "list")
        {
            manager.listTasks();
            return 0;
        }

        if (command == "add")
        {
            if (argc < 3)
            {
                std::cerr
                    << "Error: task text required.\n";
                return 1;
            }

            manager.addTask(
                joinArguments(
                    argc,
                    argv,
                    2
                )
            );

            manager.save(TASK_FILE);

            return 0;
        }

        if (command == "done")
        {
            if (argc != 3)
            {
                std::cerr
                    << "Error: task number required.\n";
                return 1;
            }

            auto taskNumber =
                parseTaskNumber(argv[2]);

            if (!manager.markTaskDone(
                    taskNumber))
            {
                std::cerr
                    << "Error: task not found.\n";

                return 1;
            }

            manager.save(TASK_FILE);

            return 0;
        }

        if (command == "remove")
        {
            if (argc != 3)
            {
                std::cerr
                    << "Error: task number required.\n";
                return 1;
            }

            auto taskNumber =
                parseTaskNumber(argv[2]);

            if (!manager.removeTask(
                    taskNumber))
            {
                std::cerr
                    << "Error: task not found.\n";

                return 1;
            }

            manager.save(TASK_FILE);

            return 0;
        }

        std::cerr
            << "Error: unknown command.\n";

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