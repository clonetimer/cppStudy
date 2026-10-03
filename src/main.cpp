#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(
    int argc,
    char* argv[]
)
{
    if (argc < 2)
    {
        std::cerr
            << "Usage: run <command> [args...]\n";

        return 1;
    }

    pid_t pid = fork();

    if (pid == -1)
    {
        std::cerr
            << "fork failed: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    if (pid == 0)
    {
        execvp(
            argv[1],
            &argv[1]
        );

        // execvp 成功不会返回
        std::cerr
            << "exec failed: "
            << std::strerror(errno)
            << '\n';

        _exit(127);
    }

    int status = 0;

    pid_t result =
        waitpid(
            pid,
            &status,
            0
        );

    if (result == -1)
    {
        std::cerr
            << "waitpid failed: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    if (WIFEXITED(status))
    {
        int exitCode =
            WEXITSTATUS(status);

        std::cout
            << "Child exited with code "
            << exitCode
            << '\n';

        return exitCode;
    }

    if (WIFSIGNALED(status))
    {
        std::cout
            << "Child terminated by signal "
            << WTERMSIG(status)
            << '\n';

        return 1;
    }

    return 1;
}