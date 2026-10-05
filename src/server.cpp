#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

bool setNonBlocking(int fd)
{
    int flags = fcntl(
        fd,
        F_GETFL,
        0
    );

    if (flags == -1)
    {
        return false;
    }

    return fcntl(
        fd,
        F_SETFL,
        flags | O_NONBLOCK
    ) != -1;
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
        listen(serverFd, 128)
        == -1
    )
    {
        std::cerr
            << "listen failed\n";

        close(serverFd);
        return 1;
    }

    if (!setNonBlocking(serverFd))
    {
        std::cerr
            << "failed to set server nonblocking\n";

        close(serverFd);
        return 1;
    }

    std::vector<int> clients;

    std::cout
        << "Non-blocking server listening on 8080\n";

    while (true)
    {
        // 1. 尽量 accept 新连接
        while (true)
        {
            int clientFd = accept(
                serverFd,
                nullptr,
                nullptr
            );

            if (clientFd >= 0)
            {
                if (!setNonBlocking(clientFd))
                {
                    close(clientFd);
                    continue;
                }

                clients.push_back(
                    clientFd
                );

                std::cout
                    << "Client connected, fd="
                    << clientFd
                    << '\n';

                continue;
            }

            if (
                errno == EAGAIN ||
                errno == EWOULDBLOCK
            )
            {
                break;
            }

            std::cerr
                << "accept failed: "
                << std::strerror(errno)
                << '\n';

            break;
        }

        // 2. 检查每一个 client
        for (auto it = clients.begin();
             it != clients.end();)
        {
            int clientFd = *it;

            char buffer[4096];

            ssize_t received =
                recv(
                    clientFd,
                    buffer,
                    sizeof(buffer),
                    0
                );

            if (received > 0)
            {
                std::cout
                    << "fd "
                    << clientFd
                    << " received "
                    << received
                    << " bytes\n";

                ++it;
            }
            else if (received == 0)
            {
                std::cout
                    << "Client disconnected, fd="
                    << clientFd
                    << '\n';

                close(clientFd);

                it = clients.erase(it);
            }
            else if (
                errno == EAGAIN ||
                errno == EWOULDBLOCK
            )
            {
                // 没数据，正常情况
                ++it;
            }
            else
            {
                std::cerr
                    << "recv error on fd "
                    << clientFd
                    << ": "
                    << std::strerror(errno)
                    << '\n';

                close(clientFd);

                it = clients.erase(it);
            }
        }
    }
}