#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    int serverFd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (serverFd == -1)
    {
        std::cerr
            << "socket failed: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    int option = 1;

    setsockopt(
        serverFd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &option,
        sizeof(option)
    );

    sockaddr_in address{};

    address.sin_family = AF_INET;

    address.sin_addr.s_addr =
        htonl(INADDR_ANY);

    address.sin_port =
        htons(8080);

    if (
        bind(
            serverFd,
            reinterpret_cast<sockaddr*>(
                &address
            ),
            sizeof(address)
        ) == -1
    )
    {
        std::cerr
            << "bind failed: "
            << std::strerror(errno)
            << '\n';

        close(serverFd);
        return 1;
    }

    if (
        listen(
            serverFd,
            16
        ) == -1
    )
    {
        std::cerr
            << "listen failed: "
            << std::strerror(errno)
            << '\n';

        close(serverFd);
        return 1;
    }

    std::cout
        << "Server listening on port 8080\n";

    int clientFd = accept(
        serverFd,
        nullptr,
        nullptr
    );

    if (clientFd == -1)
    {
        std::cerr
            << "accept failed: "
            << std::strerror(errno)
            << '\n';

        close(serverFd);
        return 1;
    }

    const char message[] =
        "Hello from server\n";

    send(
        clientFd,
        message,
        sizeof(message) - 1,
        0
    );

    while (true)
    {
        char buffer[1024];

        ssize_t bytesRead = recv(
            clientFd,
            buffer,
            sizeof(buffer) - 1,
            0
        );

        if (bytesRead == -1)
        {
            std::cerr
                << "recv failed: "
                << std::strerror(errno)
                << '\n';

            break;
        }

        if (bytesRead == 0)
        {
            std::cout
                << "Client disconnected\n";

            break;
        }

        buffer[bytesRead] = '\0';

        std::cout
            << "Received from client: "
            << buffer
            << '\n';
    }

    close(clientFd);
    close(serverFd);

    return 0;
}