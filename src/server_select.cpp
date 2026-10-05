#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

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
            reinterpret_cast<sockaddr*>(&address),
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

    if (listen(serverFd, 128) == -1)
    {
        std::cerr
            << "listen failed\n";

        close(serverFd);
        return 1;
    }

    std::vector<int> clients;

    std::cout
        << "select server listening on 8080\n";

    while (true)
    {
        fd_set readSet;

        FD_ZERO(&readSet);

        FD_SET(
            serverFd,
            &readSet
        );

        int maxFd = serverFd;

        for (int clientFd : clients)
        {
            FD_SET(
                clientFd,
                &readSet
            );

            if (clientFd > maxFd)
            {
                maxFd = clientFd;
            }
        }

        int ready = select(
            maxFd + 1,
            &readSet,
            nullptr,
            nullptr,
            nullptr
        );

        if (ready == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr
                << "select failed: "
                << std::strerror(errno)
                << '\n';

            break;
        }

        // listening socket ready
        if (
            FD_ISSET(
                serverFd,
                &readSet
            )
        )
        {
            int clientFd = accept(
                serverFd,
                nullptr,
                nullptr
            );

            if (clientFd >= 0)
            {
                clients.push_back(
                    clientFd
                );

                std::cout
                    << "Client connected, fd="
                    << clientFd
                    << '\n';
            }
        }

        // client sockets ready
        for (auto it = clients.begin();
             it != clients.end();)
        {
            int clientFd = *it;

            if (
                !FD_ISSET(
                    clientFd,
                    &readSet
                )
            )
            {
                ++it;
                continue;
            }

            char buffer[4096];

            ssize_t received = recv(
                clientFd,
                buffer,
                sizeof(buffer),
                0
            );

            if (received > 0)
            {
                send(
                    clientFd,
                    buffer,
                    static_cast<std::size_t>(
                        received
                    ),
                    MSG_NOSIGNAL
                );

                ++it;
            }
            else
            {
                if (received == 0)
                {
                    std::cout
                        << "Client disconnected, fd="
                        << clientFd
                        << '\n';
                }
                else
                {
                    std::cerr
                        << "recv error, fd="
                        << clientFd
                        << ": "
                        << std::strerror(errno)
                        << '\n';
                }

                close(clientFd);

                it = clients.erase(it);
            }
        }
    }

    for (int clientFd : clients)
    {
        close(clientFd);
    }

    close(serverFd);

    return 0;
}