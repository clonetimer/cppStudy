#include <fcntl.h>
#include <iostream>
#include <unistd.h>

bool writeAll(
    int fd,
    const char* buffer,
    std::size_t size
)
{
    std::size_t totalWritten = 0;

    while (totalWritten < size)
    {
        ssize_t written =
            write(
                fd,
                buffer + totalWritten,
                size - totalWritten
            );

        if (written == -1)
        {
            return false;
        }

        totalWritten +=
            static_cast<std::size_t>(written);
    }

    return true;
}

int main()
{
    int fd = open(
        "scores.txt",
        O_RDONLY
    );

    if (fd == -1)
    {
        std::cerr
            << "Failed to open file\n";
        return 1;
    }

    char buffer[128];

    while (true)
    {
        ssize_t bytesRead =
            read(
                fd,
                buffer,
                sizeof(buffer)
            );

        if (bytesRead > 0)
        {
            if (!writeAll(STDOUT_FILENO, buffer, static_cast<std::size_t>(bytesRead)))
            {
                std::cerr
                    << "Write failed\n";

                close(fd);
                return 1;
            }
        }
        else if (bytesRead == 0)
        {
            break;
        }
        else
        {
            std::cerr
                << "Read failed\n";

            close(fd);
            return 1;
        }
    }

    close(fd);

    return 0;
}