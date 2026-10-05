#include <arpa/inet.h>
#include <sys/socket.h>

#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <thread>

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

void handleClient(int clinetFd)
{
    char buffer[4096];


    while (true)
    {
        ssize_t received = recv(
            clinetFd,
            buffer,
            sizeof(buffer),
            0
        );

        if (received == -1)
        {
            std::cerr
                << "recv failed: "
                << std::strerror(errno)
                << '\n';

            break;
        }
        else if (received == 0)
        {
            std::cout
                << "client disconnected\n";

            break;
        }

        if (!sendAll(clinetFd, buffer, received))
        {
            break;
        }
    }
}

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

    sockaddr_in serverAddress{};

    serverAddress.sin_family =
        AF_INET;
    serverAddress.sin_port =
        htons(8080);
    serverAddress.sin_addr.s_addr =
        INADDR_ANY;
    if (
        bind(
            serverFd,
            reinterpret_cast<sockaddr*>(
                &serverAddress
            ),
            sizeof(serverAddress)
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
            SOMAXCONN
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
        << "server listening on port 8080\n";

    while (true)
    {
        sockaddr_in clientAddress{};
        socklen_t clientAddressSize =
            sizeof(clientAddress);

        int clientFd = accept(
            serverFd,
            reinterpret_cast<sockaddr*>(
                &clientAddress
            ),
            &clientAddressSize
        );

        if (clientFd == -1)
        {
            std::cerr
                << "accept failed: "
                << std::strerror(errno)
                << '\n';

            continue;
        }

        char ip[INET_ADDRSTRLEN];

        inet_ntop(
            AF_INET,
            &clientAddress.sin_addr,
            ip,
            sizeof(ip)
        );

        std::cout
            << "Client connected: "
            << ip
            << ':'
            << ntohs(clientAddress.sin_port)
            << '\n';

        std::thread(
            handleClient,
            clientFd
        ).detach();
    }

    close(serverFd);
    return 0;
    
}