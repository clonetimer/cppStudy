#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    int socketFd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (socketFd == -1)
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

    if (
        inet_pton(
            AF_INET,
            "127.0.0.1",
            &serverAddress.sin_addr
        ) != 1
    )
    {
        std::cerr
            << "invalid address\n";

        close(socketFd);
        return 1;
    }

    if (
        connect(
            socketFd,
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

        close(socketFd);
        return 1;
    }

    char buffer[1024];

    ssize_t received =
        recv(
            socketFd,
            buffer,
            sizeof(buffer),
            0
        );

    if (received > 0)
    {
        std::cout.write(
            buffer,
            received
        );
    }

    while (true)
    {
        std::string input;

        std::cout
            << "Enter message: ";
        std::getline(
            std::cin,
            input
        );

        if (input.empty())
        {
            break;
        }

        send(
            socketFd,
            input.c_str(),
            input.size(),
            0
        );
    }

    close(socketFd);

    return 0;
}