#include "connection.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unordered_map>
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

bool updateEvents(
    int epollFd,
    const Connection& connection
)
{
    epoll_event event{};

    event.data.fd = connection.fd;

    event.events = EPOLLIN;

    if (!connection.writeBuffer.empty())
    {
        event.events |= EPOLLOUT;
    }

    return epoll_ctl(
        epollFd,
        EPOLL_CTL_MOD,
        connection.fd,
        &event
    ) != -1;
}

void broadcast(
    int epollFd,
    std::unordered_map<int, Connection>& connections,
    int senderFd,
    const std::string& message
)
{
    for (auto& [fd, connection] : connections)
    {
        if (fd == senderFd)
        {
            continue;
        }

        connection.writeBuffer += message;

        updateEvents(
            epollFd,
            connection
        );
    }
}

bool flushWriteBuffer(
    int epollFd,
    Connection& connection
)
{
    while (!connection.writeBuffer.empty())
    {
        ssize_t sent = send(
            connection.fd,
            connection.writeBuffer.data(),
            connection.writeBuffer.size(),
            MSG_NOSIGNAL
        );

        if (sent > 0)
        {
            connection.writeBuffer.erase(
                0,
                static_cast<std::size_t>(sent)
            );

            continue;
        }

        if (sent == -1 &&
            (
                errno == EAGAIN ||
                errno == EWOULDBLOCK
            ))
        {
            break;
        }

        return false;
    }

    return updateEvents(
        epollFd,
        connection
    );
}

std::vector<std::string> extractLines(
    std::string& buffer
)
{
    std::vector<std::string> lines;

    while (true)
    {
        std::size_t position =
            buffer.find('\n');

        if (position == std::string::npos)
        {
            break;
        }

        std::string line =
            buffer.substr(
                0,
                position
            );

        buffer.erase(
            0,
            position + 1
        );

        if (!line.empty() &&
            line.back() == '\r')
        {
            line.pop_back();
        }

        lines.push_back(
            std::move(line)
        );
    }

    return lines;
}

void handleLine(
    int epollFd,
    std::unordered_map<int, Connection>& connections,
    Connection& connection,
    const std::string& line
)
{
    if (!connection.registered)
    {
        const std::string prefix =
            "NICK ";

        if (
            line.rfind(
                prefix,
                0
            ) != 0
        )
        {
            connection.writeBuffer +=
                "ERROR first command must be NICK\n";

            updateEvents(
                epollFd,
                connection
            );

            return;
        }

        std::string nickname =
            line.substr(
                prefix.size()
            );

        if (nickname.empty())
        {
            connection.writeBuffer +=
                "ERROR empty nickname\n";

            updateEvents(
                epollFd,
                connection
            );

            return;
        }

        connection.nickname =
            std::move(nickname);

        connection.registered = true;

        connection.writeBuffer +=
            "Welcome "
            + connection.nickname
            + "\n";

        updateEvents(
            epollFd,
            connection
        );

        broadcast(
            epollFd,
            connections,
            connection.fd,
            "*** "
                + connection.nickname
                + " joined ***\n"
        );

        return;
    }

    if (line == "/quit")
    {
        return;
    }

    if (line.empty())
    {
        return;
    }

    broadcast(
        epollFd,
        connections,
        connection.fd,
        "["
            + connection.nickname
            + "] "
            + line
            + "\n"
    );
}

bool handleRead(
    int epollFd,
    std::unordered_map<int, Connection>& connections,
    Connection& connection
)
{
    char buffer[4096];

    while (true)
    {
        ssize_t received = recv(
            connection.fd,
            buffer,
            sizeof(buffer),
            0
        );

        if (received > 0)
        {
            connection.readBuffer.append(
                buffer,
                static_cast<std::size_t>(
                    received
                )
            );

            auto lines =
                extractLines(
                    connection.readBuffer
                );

            for (const auto& line : lines)
            {
                if (
                    connection.registered &&
                    line == "/quit"
                )
                {
                    return false;
                }

                handleLine(
                    epollFd,
                    connections,
                    connection,
                    line
                );
            }

            continue;
        }

        if (received == 0)
        {
            return false;
        }

        if (
            errno == EAGAIN ||
            errno == EWOULDBLOCK
        )
        {
            return true;
        }

        return false;
    }
}

void removeConnection(
    int epollFd,
    std::unordered_map<int, Connection>& connections,
    int fd
)
{
    auto it =
        connections.find(fd);

    if (it == connections.end())
    {
        return;
    }

    std::string nickname =
        it->second.nickname;

    bool wasRegistered =
        it->second.registered;

    epoll_ctl(
        epollFd,
        EPOLL_CTL_DEL,
        fd,
        nullptr
    );

    close(fd);

    connections.erase(it);

    if (wasRegistered)
    {
        broadcast(
            epollFd,
            connections,
            -1,
            "*** "
                + nickname
                + " left ***\n"
        );
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

    sockaddr_in address{};

    address.sin_family =
        AF_INET;

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
        close(serverFd);

        return 1;
    }

    if (!setNonBlocking(serverFd))
    {
        close(serverFd);

        return 1;
    }

    int epollFd =
        epoll_create1(0);

    if (epollFd == -1)
    {
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
        close(epollFd);
        close(serverFd);

        return 1;
    }

    std::unordered_map<
        int,
        Connection
    > connections;

    constexpr int MaxEvents = 64;

    epoll_event events[MaxEvents];

    std::cout
        << "Chat server listening on port 8080\n";

    while (true)
    {
        int count = epoll_wait(
            epollFd,
            events,
            MaxEvents,
            -1
        );

        if (count == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            break;
        }

        for (int i = 0;
             i < count;
             ++i)
        {
            int fd =
                events[i].data.fd;

            std::uint32_t eventMask =
                events[i].events;

            if (fd == serverFd)
            {
                while (true)
                {
                    sockaddr_in clientAddress{};

                    socklen_t clientLength =
                        sizeof(
                            clientAddress
                        );

                    int clientFd =
                        accept(
                            serverFd,
                            reinterpret_cast<
                                sockaddr*
                            >(
                                &clientAddress
                            ),
                            &clientLength
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

                        epoll_event event{};

                        event.events =
                            EPOLLIN;

                        event.data.fd =
                            clientFd;

                        if (
                            epoll_ctl(
                                epollFd,
                                EPOLL_CTL_ADD,
                                clientFd,
                                &event
                            ) == -1
                        )
                        {
                            close(clientFd);

                            continue;
                        }

                        Connection connection;

                        connection.fd =
                            clientFd;

                        connections.emplace(
                            clientFd,
                            std::move(connection)
                        );

                        std::cout
                            << "Connected fd="
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

                    break;
                }

                continue;
            }

            auto it =
                connections.find(fd);

            if (
                it ==
                connections.end()
            )
            {
                continue;
            }

            bool keep =
                true;

            if (
                eventMask
                & (
                    EPOLLERR |
                    EPOLLHUP
                )
            )
            {
                keep = false;
            }

            if (
                keep &&
                (
                    eventMask
                    & EPOLLIN
                )
            )
            {
                keep = handleRead(
                    epollFd,
                    connections,
                    it->second
                );
            }

            if (
                keep &&
                (
                    eventMask
                    & EPOLLOUT
                )
            )
            {
                keep =
                    flushWriteBuffer(
                        epollFd,
                        it->second
                    );
            }

            if (!keep)
            {
                removeConnection(
                    epollFd,
                    connections,
                    fd
                );
            }
        }
    }

    for (
        auto& [fd, connection] :
        connections
    )
    {
        (void)connection;
        close(fd);
    }

    close(epollFd);
    close(serverFd);

    return 0;
}