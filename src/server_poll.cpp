#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>
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

    if (
        listen(
            serverFd,
            128
        ) == -1
    )
    {
        std::cerr
            << "listen failed\n";

        close(serverFd);
        return 1;
    }

    std::vector<pollfd> fds;

    pollfd serverPollFd{};

    serverPollFd.fd = serverFd;
    serverPollFd.events = POLLIN;

    fds.push_back(
        serverPollFd
    );

    std::cout
        << "poll server listening on 8080\n";

    while (true)
    {
        int ready = poll(
            fds.data(),
            fds.size(),
            -1
        );

        if (ready == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr
                << "poll failed: "
                << std::strerror(errno)
                << '\n';

            break;
        }

        // listening socket
        if (
            fds[0].revents
            & POLLIN
        )
        {
            int clientFd = accept(
                serverFd,
                nullptr,
                nullptr
            );

            if (clientFd >= 0)
            {
                pollfd clientPollFd{};

                clientPollFd.fd =
                    clientFd;

                clientPollFd.events =
                    POLLIN;

                fds.push_back(
                    clientPollFd
                );

                std::cout
                    << "Client connected, fd="
                    << clientFd
                    << '\n';
            }
        }

        for (std::size_t i = 1;
             i < fds.size();)
        {
            short events =
                fds[i].revents;

            if (
                events
                & (
                    POLLERR |
                    POLLHUP |
                    POLLNVAL
                )
            )
            {
                close(
                    fds[i].fd
                );

                fds.erase(
                    fds.begin()
                    + static_cast<
                        std::ptrdiff_t
                    >(i)
                );

                continue;
            }

            if (
                events
                & POLLIN
            )
            {
                char buffer[4096];

                ssize_t received =
                    recv(
                        fds[i].fd,
                        buffer,
                        sizeof(buffer),
                        0
                    );

                if (received > 0)
                {
                    send(
                        fds[i].fd,
                        buffer,
                        static_cast<
                            std::size_t
                        >(received),
                        MSG_NOSIGNAL
                    );
                }
                else
                {
                    close(
                        fds[i].fd
                    );

                    fds.erase(
                        fds.begin()
                        + static_cast<
                            std::ptrdiff_t
                        >(i)
                    );

                    continue;
                }
            }

            ++i;
        }
    }

    for (auto& entry : fds)
    {
        close(entry.fd);
    }

    return 0;
}