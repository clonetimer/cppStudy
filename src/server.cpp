#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

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
        listen(
            serverFd,
            128
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

    if (!setNonBlocking(serverFd))
    {
        std::cerr
            << "failed to set server socket non-blocking\n";

        close(serverFd);
        return 1;
    }

    int epollFd = epoll_create1(0);

    if (epollFd == -1)
    {
        std::cerr
            << "epoll_create1 failed: "
            << std::strerror(errno)
            << '\n';

        close(serverFd);
        return 1;
    }

    epoll_event serverEvent{};

    serverEvent.events =
        EPOLLIN;

    serverEvent.data.fd =
        serverFd;

    if (
        epoll_ctl(
            epollFd,
            EPOLL_CTL_ADD,
            serverFd,
            &serverEvent
        ) == -1
    )
    {
        std::cerr
            << "epoll_ctl failed: "
            << std::strerror(errno)
            << '\n';

        close(epollFd);
        close(serverFd);

        return 1;
    }

    constexpr int MaxEvents = 64;

    epoll_event events[MaxEvents];

    std::cout
        << "epoll server listening on port 8080\n";

    while (true)
    {
        int eventCount =
            epoll_wait(
                epollFd,
                events,
                MaxEvents,
                -1
            );

        if (eventCount == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr
                << "epoll_wait failed: "
                << std::strerror(errno)
                << '\n';

            break;
        }

        for (int i = 0;
             i < eventCount;
             ++i)
        {
            int fd =
                events[i].data.fd;

            std::uint32_t currentEvents =
                events[i].events;

            if (fd == serverFd)
            {
                while (true)
                {
                    int clientFd =
                        accept(
                            serverFd,
                            nullptr,
                            nullptr
                        );

                    if (clientFd >= 0)
                    {
                        if (
                            !setNonBlocking(
                                clientFd
                            )
                        )
                        {
                            close(clientFd);
                            continue;
                        }

                        epoll_event clientEvent{};

                        clientEvent.events =
                            EPOLLIN;

                        clientEvent.data.fd =
                            clientFd;

                        if (
                            epoll_ctl(
                                epollFd,
                                EPOLL_CTL_ADD,
                                clientFd,
                                &clientEvent
                            ) == -1
                        )
                        {
                            close(clientFd);
                            continue;
                        }

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

                continue;
            }

            if (
                currentEvents
                & (
                    EPOLLERR |
                    EPOLLHUP
                )
            )
            {
                epoll_ctl(
                    epollFd,
                    EPOLL_CTL_DEL,
                    fd,
                    nullptr
                );

                close(fd);

                continue;
            }

            if (
                currentEvents
                & EPOLLIN
            )
            {
                bool disconnected = false;

                while (true)
                {
                    char buffer[4096];

                    ssize_t received =
                        recv(
                            fd,
                            buffer,
                            sizeof(buffer),
                            0
                        );

                    if (received > 0)
                    {
                        // 教学版 echo：
                        // 暂时直接 send。
                        ssize_t sent =
                            send(
                                fd,
                                buffer,
                                static_cast<
                                    std::size_t
                                >(received),
                                MSG_NOSIGNAL
                            );

                        if (sent < 0)
                        {
                            if (
                                errno != EAGAIN &&
                                errno != EWOULDBLOCK
                            )
                            {
                                disconnected =
                                    true;
                            }
                        }

                        continue;
                    }

                    if (received == 0)
                    {
                        disconnected = true;
                        break;
                    }

                    if (
                        errno == EAGAIN ||
                        errno == EWOULDBLOCK
                    )
                    {
                        break;
                    }

                    disconnected = true;
                    break;
                }

                if (disconnected)
                {
                    std::cout
                        << "Client disconnected, fd="
                        << fd
                        << '\n';

                    epoll_ctl(
                        epollFd,
                        EPOLL_CTL_DEL,
                        fd,
                        nullptr
                    );

                    close(fd);
                }
            }
        }
    }

    close(epollFd);
    close(serverFd);

    return 0;
}