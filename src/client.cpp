#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

bool sendAll(
    int fd,
    const char* data,
    std::size_t size
)
{
    std::size_t total = 0;

    while (total < size)
    {
        ssize_t sent = send(
            fd,
            data + total,
            size - total,
            0
        );

        if (sent == -1)
        {
            std::cerr
                << "send failed: "
                << std::strerror(errno)
                << '\n';

            return false;
        }

        total += sent;
    }
    return true;
}


int main()
{
    int fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (fd == -1)
    {
        std::cerr
            << "socket failed: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    sockaddr_in serverAddress{};

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_port =
        htons(8080);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &serverAddress.sin_addr
    );

    if (
        connect(
            fd,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)
        ) == -1
    )
    {
        std::cerr
            << "connect failed: "
            << std::strerror(errno)
            << '\n';

        close(fd);
        return 1;
    }

    std::string line;

    while (
        std::getline(
            std::cin,
            line
        )
    )
    {
        line += '\n';

        if (!sendAll(
            fd,
            line.data(),
            line.size()
        ))
        {
            break;
        }

        char buffer[4096];

        ssize_t received = recv(
            fd,
            buffer,
            sizeof(buffer),
            0
        );

        if (received <= 0)
        {
            break;
        }

        std::cout.write(
            buffer,
            received
        );
    }

    close(fd);

    return 0;
}