#include "api_handlers.h"
#include "connection.h"
#include "http_request.h"
#include "http_request_parser.h"
#include "http_response.h"
#include "router.h"
#include "static_file_handler.h"
#include "thread_pool.h"
#include "thread_safe_queue.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <utility>

namespace
{

constexpr std::size_t
    MaxConnectionReadBufferSize =
        2 * 1024 * 1024;

constexpr std::size_t
    MaxConnectionWriteBufferSize =
        16 * 1024 * 1024;

constexpr std::size_t
    MaxPendingTasks =
        1024;

struct HttpTask
{
    int fd = -1;

    std::uint64_t
        connectionId = 0;

    HttpRequest request;
};

struct HttpCompletion
{
    int fd = -1;

    std::uint64_t
        connectionId = 0;

    HttpResponse response;

    bool closeAfterWrite = false;
};

enum class ReadResult
{
    Ok,
    PeerClosed,
    Error
};

std::string toLower(
    std::string_view value
)
{
    std::string result;

    result.reserve(
        value.size()
    );

    for (char ch : value)
    {
        if (
            ch >= 'A'
            &&
            ch <= 'Z'
        )
        {
            result.push_back(
                static_cast<char>(
                    ch - 'A' + 'a'
                )
            );
        }
        else
        {
            result.push_back(ch);
        }
    }

    return result;
}

bool setNonBlocking(
    int fd
)
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

HttpResponse makeTextResponse(
    int statusCode,
    std::string reasonPhrase,
    std::string body
)
{
    HttpResponse response;

    response.statusCode =
        statusCode;

    response.reasonPhrase =
        std::move(
            reasonPhrase
        );

    response.headers[
        "Content-Type"
    ] =
        "text/plain; charset=utf-8";

    response.body =
        std::move(body);

    return response;
}

std::string extractPath(
    std::string_view target
)
{
    const std::size_t query =
        target.find('?');

    if (
        query ==
        std::string_view::npos
    )
    {
        return std::string(
            target
        );
    }

    return std::string(
        target.substr(
            0,
            query
        )
    );
}

HttpResponse dispatchRequest(
    const HttpRequest& request,
    const Router& router,
    const StaticFileHandler&
        staticFiles
)
{
    const std::string path =
        extractPath(
            request.target
        );

    if (
        router.hasRoute(
            request.method,
            path
        )
    )
    {
        return router.handle(
            request
        );
    }

    return staticFiles.handle(
        request
    );
}

bool requestWantsClose(
    const HttpRequest& request
)
{
    /*
     * HTTP/1.0：
     * D5 版本简单处理为默认关闭。
     */
    if (
        request.version ==
        "HTTP/1.0"
    )
    {
        return true;
    }

    const auto it =
        request.headers.find(
            "connection"
        );

    if (
        it ==
        request.headers.end()
    )
    {
        return false;
    }

    return toLower(
        it->second
    ) == "close";
}

bool responseWantsClose(
    const HttpResponse& response
)
{
    for (
        const auto& [name, value] :
        response.headers
    )
    {
        if (
            toLower(name)
            == "connection"
        )
        {
            return toLower(
                value
            ) == "close";
        }
    }

    return false;
}

bool updateConnectionEvents(
    int epollFd,
    const Connection& connection
)
{
    epoll_event event{};

    event.data.fd =
        connection.fd;

    event.events =
        EPOLLRDHUP;

    /*
     * 已经决定关闭以后，
     * 不再读取新 HTTP Request。
     */
    if (
        !connection.closeAfterWrite
    )
    {
        event.events |=
            EPOLLIN;
    }

    if (
        !connection.writeBuffer.empty()
    )
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

void disconnectConnection(
    int epollFd,
    std::unordered_map<
        int,
        Connection
    >& connections,
    int fd
)
{
    const auto it =
        connections.find(fd);

    if (
        it ==
        connections.end()
    )
    {
        return;
    }

    epoll_ctl(
        epollFd,
        EPOLL_CTL_DEL,
        fd,
        nullptr
    );

    close(fd);

    std::cout
        << "Disconnected fd="
        << fd
        << " connectionId="
        << it->second.id
        << '\n';

    connections.erase(it);
}

bool appendResponse(
    int epollFd,
    Connection& connection,
    const HttpResponse& response,
    bool closeAfterWrite
)
{
    const std::string serialized =
        serializeResponse(
            response
        );

    if (
        serialized.size()
            >
            MaxConnectionWriteBufferSize
        ||
        connection.writeBuffer.size()
            >
            MaxConnectionWriteBufferSize
                - serialized.size()
    )
    {
        return false;
    }

    connection.writeBuffer +=
        serialized;

    if (closeAfterWrite)
    {
        connection.closeAfterWrite =
            true;
    }

    return updateConnectionEvents(
        epollFd,
        connection
    );
}

void notifyEventLoop(
    int wakeFd
)
{
    const std::uint64_t one = 1;

    while (true)
    {
        const ssize_t result =
            write(
                wakeFd,
                &one,
                sizeof(one)
            );

        if (
            result ==
            static_cast<ssize_t>(
                sizeof(one)
            )
        )
        {
            return;
        }

        if (
            result == -1
            &&
            errno == EINTR
        )
        {
            continue;
        }

        /*
         * EAGAIN 表示 eventfd counter 已经很大。
         *
         * 但这意味着 wakeFd 本身已经 readable，
         * event loop 本来就会被唤醒，因此不需要重试。
         */
        if (
            result == -1
            &&
            (
                errno == EAGAIN
                ||
                errno == EWOULDBLOCK
            )
        )
        {
            return;
        }

        return;
    }
}

void drainWakeFd(
    int wakeFd
)
{
    std::uint64_t value = 0;

    while (true)
    {
        const ssize_t result =
            read(
                wakeFd,
                &value,
                sizeof(value)
            );

        if (
            result ==
            static_cast<ssize_t>(
                sizeof(value)
            )
        )
        {
            continue;
        }

        if (
            result == -1
            &&
            errno == EINTR
        )
        {
            continue;
        }

        if (
            result == -1
            &&
            (
                errno == EAGAIN
                ||
                errno == EWOULDBLOCK
            )
        )
        {
            return;
        }

        return;
    }
}

ReadResult readFromConnection(
    Connection& connection
)
{
    char buffer[8192];

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
                    MaxConnectionReadBufferSize
                ||
                connection.readBuffer.size()
                    >
                    MaxConnectionReadBufferSize
                        - size
            )
            {
                std::cerr
                    << "Read buffer limit exceeded "
                    << "on fd="
                    << connection.fd
                    << '\n';

                return ReadResult::Error;
            }

            connection.readBuffer.append(
                buffer,
                size
            );

            continue;
        }

        if (received == 0)
        {
            return ReadResult::PeerClosed;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (
            errno == EAGAIN
            ||
            errno == EWOULDBLOCK
        )
        {
            return ReadResult::Ok;
        }

        std::cerr
            << "recv failed on fd="
            << connection.fd
            << ": "
            << std::strerror(errno)
            << '\n';

        return ReadResult::Error;
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
            sent == -1
            &&
            errno == EINTR
        )
        {
            continue;
        }

        if (
            sent == -1
            &&
            (
                errno == EAGAIN
                ||
                errno == EWOULDBLOCK
            )
        )
        {
            return updateConnectionEvents(
                epollFd,
                connection
            );
        }

        std::cerr
            << "send failed on fd="
            << connection.fd
            << ": "
            << std::strerror(errno)
            << '\n';

        return false;
    }

    /*
     * Response 已经全部发送完。
     */
    if (
        connection.closeAfterWrite
    )
    {
        return false;
    }

    return updateConnectionEvents(
        epollFd,
        connection
    );
}

/*
 * 将一个完整 HTTP Request 投递到线程池。
 *
 * 返回 false：
 * connection 应立即断开。
 */
bool tryDispatchRequest(
    int epollFd,
    Connection& connection,
    const HttpRequestParser& parser,
    ThreadPool& threadPool,
    const Router& router,
    const StaticFileHandler&
        staticFiles,
    ThreadSafeQueue<
        HttpCompletion
    >& completionQueue,
    int wakeFd
)
{
    /*
     * 一个 connection 暂时最多
     * 一个 in-flight request。
     */
    if (
        connection.processing
        ||
        connection.closeAfterWrite
    )
    {
        return true;
    }

    ParseResult result =
        parser.parse(
            connection.readBuffer
        );

    if (
        result.status ==
        ParseStatus::NeedMoreData
    )
    {
        return true;
    }

    if (
        result.status ==
        ParseStatus::Error
    )
    {
        std::cerr
            << "HTTP parse error fd="
            << connection.fd
            << ": "
            << result.error
            << '\n';

        HttpResponse response =
            makeTextResponse(
                400,
                "Bad Request",
                "400 Bad Request\n"
            );

        response.headers[
            "Connection"
        ] = "close";

        return appendResponse(
            epollFd,
            connection,
            response,
            true
        );
    }

    /*
     * Request 已经完整。
     *
     * 先从 readBuffer 移除。
     *
     * 后面可能已经存在下一个
     * pipelined request，暂时保留。
     */
    connection.readBuffer.erase(
        0,
        result.consumed
    );

    HttpTask task;

    task.fd =
        connection.fd;

    task.connectionId =
        connection.id;

    task.request =
        std::move(
            result.request
        );

    connection.processing =
        true;

    const bool accepted =
        threadPool.submit(
            [
                task =
                    std::move(task),
                &router,
                &staticFiles,
                &completionQueue,
                wakeFd
            ]() mutable
            {
                HttpCompletion
                    completion;

                completion.fd =
                    task.fd;

                completion.connectionId =
                    task.connectionId;

                try
                {
                    completion.response =
                        dispatchRequest(
                            task.request,
                            router,
                            staticFiles
                        );

                    completion.closeAfterWrite =
                        requestWantsClose(
                            task.request
                        )
                        ||
                        responseWantsClose(
                            completion.response
                        );

                    if (
                        completion.closeAfterWrite
                    )
                    {
                        completion.response.headers[
                            "Connection"
                        ] = "close";
                    }
                }
                catch (
                    const std::exception&
                    error
                )
                {
                    std::cerr
                        << "handler exception: "
                        << error.what()
                        << '\n';

                    completion.response =
                        makeTextResponse(
                            500,
                            "Internal Server Error",
                            "500 Internal Server Error\n"
                        );

                    completion.response.headers[
                        "Connection"
                    ] = "close";

                    completion.closeAfterWrite =
                        true;
                }
                catch (...)
                {
                    std::cerr
                        << "unknown handler exception\n";

                    completion.response =
                        makeTextResponse(
                            500,
                            "Internal Server Error",
                            "500 Internal Server Error\n"
                        );

                    completion.response.headers[
                        "Connection"
                    ] = "close";

                    completion.closeAfterWrite =
                        true;
                }

                completionQueue.push(
                    std::move(
                        completion
                    )
                );

                notifyEventLoop(
                    wakeFd
                );
            }
        );

    if (accepted)
    {
        return true;
    }

    /*
     * ThreadPool queue 满。
     *
     * Request 已经从 readBuffer 消费，
     * 因此直接在 Event Loop 构造 503。
     */
    connection.processing =
        false;

    HttpResponse response =
        makeTextResponse(
            503,
            "Service Unavailable",
            "503 Service Unavailable\n"
        );

    response.headers[
        "Connection"
    ] = "close";

    return appendResponse(
        epollFd,
        connection,
        response,
        true
    );
}

void handleCompletions(
    int epollFd,
    std::unordered_map<
        int,
        Connection
    >& connections,
    const HttpRequestParser& parser,
    ThreadPool& threadPool,
    const Router& router,
    const StaticFileHandler&
        staticFiles,
    ThreadSafeQueue<
        HttpCompletion
    >& completionQueue,
    int wakeFd
)
{
    HttpCompletion completion;

    while (
        completionQueue.tryPop(
            completion
        )
    )
    {
        const auto it =
            connections.find(
                completion.fd
            );

        /*
         * 客户端在 Worker 执行期间
         * 已经断开。
         */
        if (
            it ==
            connections.end()
        )
        {
            continue;
        }

        Connection& connection =
            it->second;

        /*
         * 防止 fd reuse。
         */
        if (
            connection.id !=
            completion.connectionId
        )
        {
            continue;
        }

        connection.processing =
            false;

        /*
         * 对端已经关闭发送方向，
         * 当前 Response 发完以后关闭。
         */
        const bool closeAfterWrite =
            completion.closeAfterWrite
            ||
            connection.peerClosed;

        if (
            !appendResponse(
                epollFd,
                connection,
                completion.response,
                closeAfterWrite
            )
        )
        {
            disconnectConnection(
                epollFd,
                connections,
                completion.fd
            );

            continue;
        }

        /*
         * 如果这个 Response 不要求关闭，
         * readBuffer 中可能已经存在：
         *
         * Request B
         *
         * 因此不用等新的 EPOLLIN，
         * 主动再次尝试 parse。
         */
        if (
            !connection.closeAfterWrite
        )
        {
            if (
                !tryDispatchRequest(
                    epollFd,
                    connection,
                    parser,
                    threadPool,
                    router,
                    staticFiles,
                    completionQueue,
                    wakeFd
                )
            )
            {
                disconnectConnection(
                    epollFd,
                    connections,
                    completion.fd
                );
            }
        }
    }
}

}

int main()
{
    const int serverFd =
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

    const int reuseAddress = 1;

    if (
        setsockopt(
            serverFd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuseAddress,
            sizeof(reuseAddress)
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
            << "failed to set "
            << "serverFd non-blocking\n";

        close(serverFd);

        return 1;
    }

    const int epollFd =
        epoll_create1(
            EPOLL_CLOEXEC
        );

    if (epollFd == -1)
    {
        std::cerr
            << "epoll_create1 failed: "
            << std::strerror(errno)
            << '\n';

        close(serverFd);

        return 1;
    }

    /*
     * Worker → Event Loop 通知 fd。
     */
    const int wakeFd =
        eventfd(
            0,
            EFD_NONBLOCK
            |
            EFD_CLOEXEC
        );

    if (wakeFd == -1)
    {
        std::cerr
            << "eventfd failed: "
            << std::strerror(errno)
            << '\n';

        close(epollFd);
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
            << "failed to register serverFd\n";

        close(wakeFd);
        close(epollFd);
        close(serverFd);

        return 1;
    }

    epoll_event wakeEvent{};

    wakeEvent.events =
        EPOLLIN;

    wakeEvent.data.fd =
        wakeFd;

    if (
        epoll_ctl(
            epollFd,
            EPOLL_CTL_ADD,
            wakeFd,
            &wakeEvent
        ) == -1
    )
    {
        std::cerr
            << "failed to register wakeFd\n";

        close(wakeFd);
        close(epollFd);
        close(serverFd);

        return 1;
    }

    /*
     * HTTP components。
     *
     * Parser 只在 event-loop thread 使用。
     */
    HttpRequestParser parser;

    Router router;

    router.addRoute(
        "GET",
        "/hello",
        helloHandler
    );

    router.addRoute(
        "POST",
        "/echo",
        echoHandler
    );

    router.addRoute(
        "GET",
        "/health",
        healthHandler
    );

    /*
     * Router 注册完成后不再修改。
     *
     * Worker threads 只进行并发 const read。
     */
    StaticFileHandler staticFiles(
        "www"
    );

    ThreadSafeQueue<
        HttpCompletion
    > completionQueue;

    const unsigned int
        hardwareThreads =
            std::thread::
                hardware_concurrency();

    const std::size_t
        workerCount =
            std::max(
                1u,
                hardwareThreads
            );

    ThreadPool threadPool(
        workerCount,
        MaxPendingTasks
    );

    std::cout
        << "Worker threads: "
        << workerCount
        << '\n';

    std::unordered_map<
        int,
        Connection
    > connections;

    std::uint64_t
        nextConnectionId = 1;

    constexpr int
        MaxEvents = 128;

    epoll_event
        events[MaxEvents];

    std::cout
        << "HTTP server listening on "
        << "0.0.0.0:8080\n";

    bool running = true;

    while (running)
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

            const std::uint32_t
                eventMask =
                    events[index].events;

            /*
             * 新连接。
             */
            if (fd == serverFd)
            {
                while (true)
                {
                    sockaddr_in
                        clientAddress{};

                    socklen_t
                        clientAddressLength =
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
                            &clientAddressLength
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

                        Connection
                            connection;

                        connection.fd =
                            clientFd;

                        connection.id =
                            nextConnectionId++;

                        connections.emplace(
                            clientFd,
                            std::move(
                                connection
                            )
                        );

                        epoll_event
                            clientEvent{};

                        clientEvent.events =
                            EPOLLIN
                            |
                            EPOLLRDHUP;

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
                            connections.erase(
                                clientFd
                            );

                            close(clientFd);

                            continue;
                        }

                        std::cout
                            << "Connected fd="
                            << clientFd
                            << " connectionId="
                            << connections[
                                clientFd
                            ].id
                            << '\n';

                        continue;
                    }

                    if (errno == EINTR)
                    {
                        continue;
                    }

                    if (
                        errno == EAGAIN
                        ||
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

            /*
             * Worker 完成。
             */
            if (fd == wakeFd)
            {
                drainWakeFd(
                    wakeFd
                );

                handleCompletions(
                    epollFd,
                    connections,
                    parser,
                    threadPool,
                    router,
                    staticFiles,
                    completionQueue,
                    wakeFd
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

            /*
             * EPOLLERR / EPOLLHUP
             * 直接视为 fatal。
             */
            if (
                eventMask
                &
                (
                    EPOLLERR
                    |
                    EPOLLHUP
                )
            )
            {
                disconnectConnection(
                    epollFd,
                    connections,
                    fd
                );

                continue;
            }

            /*
             * EPOLLIN / RDHUP：
             *
             * 先尽量读取所有剩余字节。
             */
            if (
                eventMask
                &
                (
                    EPOLLIN
                    |
                    EPOLLRDHUP
                )
            )
            {
                Connection& connection =
                    it->second;

                const ReadResult
                    readResult =
                        readFromConnection(
                            connection
                        );

                if (
                    readResult ==
                    ReadResult::Error
                )
                {
                    disconnectConnection(
                        epollFd,
                        connections,
                        fd
                    );

                    continue;
                }

                if (
                    readResult ==
                        ReadResult::PeerClosed
                    ||
                    (
                        eventMask
                        &
                        EPOLLRDHUP
                    )
                )
                {
                    connection.peerClosed =
                        true;
                }

                /*
                 * processing==true 时
                 * tryDispatchRequest 会直接返回。
                 *
                 * 但数据仍然可以继续进入 readBuffer。
                 */
                if (
                    !tryDispatchRequest(
                        epollFd,
                        connection,
                        parser,
                        threadPool,
                        router,
                        staticFiles,
                        completionQueue,
                        wakeFd
                    )
                )
                {
                    disconnectConnection(
                        epollFd,
                        connections,
                        fd
                    );

                    continue;
                }

                /*
                 * Peer 已经彻底关闭，
                 * 而且没有任何正在处理/待发送内容，
                 * 就没有继续保留连接的理由。
                 */
                if (
                    connection.peerClosed
                    &&
                    !connection.processing
                    &&
                    connection.writeBuffer.empty()
                    &&
                    connection.readBuffer.empty()
                )
                {
                    disconnectConnection(
                        epollFd,
                        connections,
                        fd
                    );

                    continue;
                }
            }

            /*
             * 上面可能 disconnect。
             * 因此重新 find。
             */
            it =
                connections.find(fd);

            if (
                it ==
                connections.end()
            )
            {
                continue;
            }

            if (
                eventMask
                &
                EPOLLOUT
            )
            {
                if (
                    !flushWriteBuffer(
                        epollFd,
                        it->second
                    )
                )
                {
                    disconnectConnection(
                        epollFd,
                        connections,
                        fd
                    );

                    continue;
                }
            }
        }
    }

    /*
     * 非常重要：
     *
     * 先停 ThreadPool。
     *
     * 防止还有 Worker 在使用 wakeFd，
     * 我们却提前 close(wakeFd)。
     */
    threadPool.shutdown();

    for (
        auto& [fd, connection] :
        connections
    )
    {
        (void)connection;

        close(fd);
    }

    connections.clear();

    close(wakeFd);
    close(epollFd);
    close(serverFd);

    return 0;
}