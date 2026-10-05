#include "chat_protocol.h"
#include "connection.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unordered_map>
#include <unistd.h>
#include <vector>

namespace
{

bool setNonBlocking(int fd)
{
    const int flags =
        fcntl(
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

    event.data.fd =
        connection.fd;

    event.events =
        EPOLLIN |
        EPOLLRDHUP;

    if (!connection.writeBuffer.empty())
    {
        event.events |=
            EPOLLOUT;
    }

    return epoll_ctl(
        epollFd,
        EPOLL_CTL_MOD,
        connection.fd,
        &event
    ) != -1;
}

bool nicknameExists(
    const std::unordered_map<
        int,
        Connection
    >& connections,
    std::string_view nickname,
    int ignoredFd
)
{
    for (
        const auto& [fd, connection] :
        connections
    )
    {
        if (fd == ignoredFd)
        {
            continue;
        }

        if (connection.registered &&
            connection.nickname ==
                nickname)
        {
            return true;
        }
    }

    return false;
}

bool queueMessage(
    int epollFd,
    Connection& connection,
    std::string_view message
)
{
    /*
     * 防止 size_t 加法溢出，同时限制
     * 慢客户端无限积压 writeBuffer。
     */
    if (
        message.size()
            > chat::MaxWriteBufferSize
        ||
        connection.writeBuffer.size()
            > chat::MaxWriteBufferSize
                - message.size()
    )
    {
        connection.closeRequested =
            true;

        return false;
    }

    connection.writeBuffer.append(
        message.data(),
        message.size()
    );

    if (
        !updateEvents(
            epollFd,
            connection
        )
    )
    {
        connection.closeRequested =
            true;

        return false;
    }

    return true;
}

void broadcast(
    int epollFd,
    std::unordered_map<
        int,
        Connection
    >& connections,
    int senderFd,
    std::string_view message
)
{
    for (
        auto& [fd, connection] :
        connections
    )
    {
        if (fd == senderFd)
        {
            continue;
        }

        if (!connection.registered)
        {
            continue;
        }

        if (connection.closeRequested)
        {
            continue;
        }

        queueMessage(
            epollFd,
            connection,
            message
        );
    }
}

void disconnectConnection(
    int epollFd,
    std::unordered_map<
        int,
        Connection
    >& connections,
    int fd,
    bool announce = true
)
{
    const auto it =
        connections.find(fd);

    if (it == connections.end())
    {
        return;
    }

    const bool wasRegistered =
        it->second.registered;

    const std::string nickname =
        it->second.nickname;

    epoll_ctl(
        epollFd,
        EPOLL_CTL_DEL,
        fd,
        nullptr
    );

    close(fd);

    connections.erase(it);

    std::cout
        << "Client disconnected, fd="
        << fd
        << '\n';

    if (
        announce &&
        wasRegistered
    )
    {
        const std::string message =
            "*** "
            + nickname
            + " left ***\n";

        broadcast(
            epollFd,
            connections,
            -1,
            message
        );
    }
}

void cleanupRequestedConnections(
    int epollFd,
    std::unordered_map<
        int,
        Connection
    >& connections
)
{
    /*
     * disconnectConnection() 可能广播 leave 消息，
     * 广播又可能使另外一个极慢客户端超过
     * writeBuffer 限制。
     *
     * 因此循环清理直到没有 closeRequested。
     */
    while (true)
    {
        std::vector<int> pending;

        for (
            const auto& [fd, connection] :
            connections
        )
        {
            if (
                connection.closeRequested
            )
            {
                pending.push_back(fd);
            }
        }

        if (pending.empty())
        {
            return;
        }

        for (int fd : pending)
        {
            disconnectConnection(
                epollFd,
                connections,
                fd
            );
        }
    }
}

bool handleLine(
    int epollFd,
    std::unordered_map<
        int,
        Connection
    >& connections,
    Connection& connection,
    const std::string& line
)
{
    if (
        line.size() >
        chat::MaxMessageLength
    )
    {
        queueMessage(
            epollFd,
            connection,
            "ERROR message too long\n"
        );

        return
            !connection.closeRequested;
    }

    if (!connection.registered)
    {
        constexpr std::string_view
            Prefix = "NICK ";

        if (
            line.rfind(
                Prefix.data(),
                0
            ) != 0
        )
        {
            queueMessage(
                epollFd,
                connection,
                "ERROR first command must be NICK <name>\n"
            );

            return
                !connection.closeRequested;
        }

        const std::string nickname =
            line.substr(
                Prefix.size()
            );

        if (
            !chat::isValidNickname(
                nickname
            )
        )
        {
            queueMessage(
                epollFd,
                connection,
                "ERROR invalid nickname\n"
            );

            return
                !connection.closeRequested;
        }

        if (
            nicknameExists(
                connections,
                nickname,
                connection.fd
            )
        )
        {
            queueMessage(
                epollFd,
                connection,
                "ERROR nickname already in use\n"
            );

            return
                !connection.closeRequested;
        }

        connection.nickname =
            nickname;

        connection.registered =
            true;

        queueMessage(
            epollFd,
            connection,
            "OK welcome "
                + connection.nickname
                + "\n"
        );

        const std::string joinMessage =
            "*** "
            + connection.nickname
            + " joined ***\n";

        broadcast(
            epollFd,
            connections,
            connection.fd,
            joinMessage
        );

        std::cout
            << "Registered fd="
            << connection.fd
            << " nickname="
            << connection.nickname
            << '\n';

        return
            !connection.closeRequested;
    }

    if (line == "/quit")
    {
        return false;
    }

    if (line.empty())
    {
        return true;
    }

    const std::string message =
        chat::makeChatMessage(
            connection.nickname,
            line
        );

    broadcast(
        epollFd,
        connections,
        connection.fd,
        message
    );

    return true;
}

bool handleRead(
    int epollFd,
    std::unordered_map<
        int,
        Connection
    >& connections,
    Connection& connection
)
{
    char buffer[4096];

    while (true)
    {
        const ssize_t received =
            recv(
                connection.fd,
                buffer,
                sizeof(buffer),
                0
            );

        if (received > 0)
        {
            const auto size =
                static_cast<std::size_t>(
                    received
                );

            if (
                size >
                    chat::MaxReadBufferSize
                ||
                connection.readBuffer.size()
                    >
                    chat::MaxReadBufferSize
                        - size
            )
            {
                return false;
            }

            connection.readBuffer.append(
                buffer,
                size
            );

            auto lines =
                chat::extractLines(
                    connection.readBuffer
                );

            for (
                const auto& line :
                lines
            )
            {
                if (
                    !handleLine(
                        epollFd,
                        connections,
                        connection,
                        line
                    )
                )
                {
                    return false;
                }
            }

            continue;
        }

        if (received == 0)
        {
            return false;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (
            errno == EAGAIN ||
            errno == EWOULDBLOCK
        )
        {
            return true;
        }

        std::cerr
            << "recv failed on fd "
            << connection.fd
            << ": "
            << std::strerror(errno)
            << '\n';

        return false;
    }
}

bool flushWriteBuffer(
    int epollFd,
    Connection& connection
)
{
    while (
        !connection.writeBuffer.empty()
    )
    {
        const ssize_t sent =
            send(
                connection.fd,
                connection.writeBuffer.data(),
                connection.writeBuffer.size(),
                MSG_NOSIGNAL
            );

        if (sent > 0)
        {
            connection.writeBuffer.erase(
                0,
                static_cast<std::size_t>(
                    sent
                )
            );

            continue;
        }

        if (
            sent == -1 &&
            errno == EINTR
        )
        {
            continue;
        }

        if (
            sent == -1 &&
            (
                errno == EAGAIN ||
                errno == EWOULDBLOCK
            )
        )
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

} // namespace

int main()
{
    int serverFd =
        socket(
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

    const int option = 1;

    if (
        setsockopt(
            serverFd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            sizeof(option)
        ) == -1
    )
    {
        std::cerr
            << "setsockopt failed: "
            << std::strerror(errno)
            << '\n';

        close(serverFd);

        return 1;
    }

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
            reinterpret_cast<
                sockaddr*
            >(&address),
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

    if (
        !setNonBlocking(
            serverFd
        )
    )
    {
        std::cerr
            << "failed to set server socket non-blocking\n";

        close(serverFd);

        return 1;
    }

    const int epollFd =
        epoll_create1(0);

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

    serverEvent.data.fd =
        serverFd;

    serverEvent.events =
        EPOLLIN;

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
            << "epoll_ctl server failed: "
            << std::strerror(errno)
            << '\n';

        close(epollFd);
        close(serverFd);

        return 1;
    }

    std::unordered_map<
        int,
        Connection
    > connections;

    constexpr int MaxEvents = 64;

    epoll_event events[
        MaxEvents
    ];

    std::cout
        << "Chat server listening on "
        << "0.0.0.0:8080\n";

    while (true)
    {
        const int eventCount =
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

        for (
            int index = 0;
            index < eventCount;
            ++index
        )
        {
            const int fd =
                events[index].data.fd;

            const std::uint32_t eventMask =
                events[index].events;

            /*
             * Listening socket ready:
             * accept 一直到 EAGAIN。
             */
            if (fd == serverFd)
            {
                while (true)
                {
                    sockaddr_in
                        clientAddress{};

                    socklen_t
                        clientLength =
                            sizeof(
                                clientAddress
                            );

                    const int clientFd =
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

                        epoll_event
                            clientEvent{};

                        clientEvent.data.fd =
                            clientFd;

                        clientEvent.events =
                            EPOLLIN |
                            EPOLLRDHUP;

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

                        Connection connection;

                        connection.fd =
                            clientFd;

                        connections.emplace(
                            clientFd,
                            std::move(
                                connection
                            )
                        );

                        char ip[
                            INET_ADDRSTRLEN
                        ]{};

                        inet_ntop(
                            AF_INET,
                            &clientAddress.sin_addr,
                            ip,
                            sizeof(ip)
                        );

                        std::cout
                            << "Connected fd="
                            << clientFd
                            << " from "
                            << ip
                            << ':'
                            << ntohs(
                                clientAddress.sin_port
                            )
                            << '\n';

                        continue;
                    }

                    if (errno == EINTR)
                    {
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

                cleanupRequestedConnections(
                    epollFd,
                    connections
                );

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

            bool keepConnection =
                true;

            /*
             * EPOLLERR / EPOLLHUP:
             * 明确作为连接失败处理。
             */
            if (
                eventMask &
                (
                    EPOLLERR |
                    EPOLLHUP
                )
            )
            {
                keepConnection =
                    false;
            }

            /*
             * 即使收到了 EPOLLRDHUP，
             * 如果还有 EPOLLIN，先把剩余数据读完。
             */
            if (
                keepConnection &&
                (
                    eventMask &
                    EPOLLIN
                )
            )
            {
                keepConnection =
                    handleRead(
                        epollFd,
                        connections,
                        it->second
                    );
            }

            /*
             * handleRead 期间 unordered_map
             * 没有 erase 当前元素，因此引用仍有效。
             */
            if (
                keepConnection &&
                (
                    eventMask &
                    EPOLLOUT
                )
            )
            {
                keepConnection =
                    flushWriteBuffer(
                        epollFd,
                        it->second
                    );
            }

            /*
             * 对端关闭写方向。
             * 当前 batch 中可读数据已经先处理。
             */
            if (
                keepConnection &&
                (
                    eventMask &
                    EPOLLRDHUP
                )
            )
            {
                keepConnection =
                    false;
            }

            if (!keepConnection)
            {
                disconnectConnection(
                    epollFd,
                    connections,
                    fd
                );
            }

            cleanupRequestedConnections(
                epollFd,
                connections
            );
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