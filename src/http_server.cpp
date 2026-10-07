#include "api_handlers.h"
#include "connection.h"
#include "http_request.h"
#include "http_request_parser.h"
#include "http_response.h"
#include "logger.h"
#include "router.h"
#include "static_file_handler.h"
#include "thread_pool.h"
#include "thread_safe_queue.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <utility>
#include <vector>

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

constexpr std::size_t
    MaxRequestsPerConnection =
        100;

constexpr std::chrono::seconds
    IdleTimeout{60};

constexpr int
    EpollWaitTimeoutMs =
        1000;

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
                    ch - 'A'
                    + 'a'
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

std::string trimAscii(
    std::string_view value
)
{
    std::size_t begin = 0;

    while (
        begin < value.size()
        &&
        (
            value[begin] == ' '
            ||
            value[begin] == '\t'
        )
    )
    {
        ++begin;
    }

    std::size_t end =
        value.size();

    while (
        end > begin
        &&
        (
            value[end - 1] == ' '
            ||
            value[end - 1] == '\t'
        )
    )
    {
        --end;
    }

    return std::string(
        value.substr(
            begin,
            end - begin
        )
    );
}

bool headerContainsToken(
    std::string_view value,
    std::string_view token
)
{
    const std::string lowerValue =
        toLower(value);

    const std::string lowerToken =
        toLower(token);

    std::size_t begin = 0;

    while (
        begin <=
        lowerValue.size()
    )
    {
        const std::size_t comma =
            lowerValue.find(
                ',',
                begin
            );

        const std::size_t end =
            comma ==
                std::string::npos
            ? lowerValue.size()
            : comma;

        const std::string current =
            trimAscii(
                std::string_view(
                    lowerValue
                ).substr(
                    begin,
                    end - begin
                )
            );

        if (
            current ==
            lowerToken
        )
        {
            return true;
        }

        if (
            comma ==
            std::string::npos
        )
        {
            break;
        }

        begin =
            comma + 1;
    }

    return false;
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

std::string reasonPhraseForStatus(
    int statusCode
)
{
    switch (statusCode)
    {
        case 400:
            return "Bad Request";

        case 413:
            return "Payload Too Large";

        case 431:
            return
                "Request Header Fields Too Large";

        case 500:
            return
                "Internal Server Error";

        case 503:
            return
                "Service Unavailable";

        default:
            return "Bad Request";
    }
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

/*
 * --------------------------------------------------
 * HTTP keep-alive semantics
 * --------------------------------------------------
 */

bool requestWantsClose(
    const HttpRequest& request
)
{
    const auto it =
        request.headers.find(
            "connection"
        );

    if (
        request.version ==
        "HTTP/1.1"
    )
    {
        /*
         * HTTP/1.1 默认 persistent。
         */
        if (
            it ==
            request.headers.end()
        )
        {
            return false;
        }

        return headerContainsToken(
            it->second,
            "close"
        );
    }

    if (
        request.version ==
        "HTTP/1.0"
    )
    {
        /*
         * HTTP/1.0 默认关闭。
         *
         * 只有明确 keep-alive
         * 才保持连接。
         */
        if (
            it ==
            request.headers.end()
        )
        {
            return true;
        }

        return !headerContainsToken(
            it->second,
            "keep-alive"
        );
    }

    return true;
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
            return headerContainsToken(
                value,
                "close"
            );
        }
    }

    return false;
}

void eraseHeader(
    HttpResponse& response,
    std::string_view headerName
)
{
    const std::string wanted =
        toLower(headerName);

    for (
        auto it =
            response.headers.begin();
        it !=
            response.headers.end();
    )
    {
        if (
            toLower(
                it->first
            ) == wanted
        )
        {
            it =
                response.headers.erase(
                    it
                );
        }
        else
        {
            ++it;
        }
    }
}

void setHeader(
    HttpResponse& response,
    std::string name,
    std::string value
)
{
    eraseHeader(
        response,
        name
    );

    response.headers[
        std::move(name)
    ] =
        std::move(value);
}

/*
 * --------------------------------------------------
 * epoll connection management
 * --------------------------------------------------
 */

bool updateConnectionEvents(
    int epollFd,
    const Connection& connection
)
{
    epoll_event event{};

    event.data.fd =
        connection.fd;

    event.events = 0;

    if (
        !connection.closeAfterWrite
        &&
        !connection.peerClosed
    )
    {
        event.events |=
            EPOLLIN
            |
            EPOLLRDHUP;
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

    const std::uint64_t
        connectionId =
            it->second.id;

    epoll_ctl(
        epollFd,
        EPOLL_CTL_DEL,
        fd,
        nullptr
    );

    close(fd);

    connections.erase(it);

    Logger::info(
        "client disconnected fd="
        + std::to_string(fd)
        + " connectionId="
        + std::to_string(
            connectionId
        )
    );
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
        Logger::warning(
            "write buffer limit exceeded fd="
            + std::to_string(
                connection.fd
            )
        );

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

/*
 * --------------------------------------------------
 * eventfd
 * --------------------------------------------------
 */

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

/*
 * --------------------------------------------------
 * socket read/write
 * --------------------------------------------------
 */

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
            connection.lastActivity =
                std::chrono::
                    steady_clock::now();

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
                Logger::warning(
                    "read buffer limit exceeded fd="
                    + std::to_string(
                        connection.fd
                    )
                );

                return
                    ReadResult::Error;
            }

            connection.readBuffer.append(
                buffer,
                size
            );

            continue;
        }

        if (received == 0)
        {
            return
                ReadResult::PeerClosed;
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

        Logger::error(
            "recv failed fd="
            + std::to_string(
                connection.fd
            )
            + ": "
            + std::strerror(errno)
        );

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
            connection.lastActivity =
                std::chrono::
                    steady_clock::now();

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

        Logger::error(
            "send failed fd="
            + std::to_string(
                connection.fd
            )
            + ": "
            + std::strerror(errno)
        );

        return false;
    }

    /*
     * 所有 Response 已发送完成。
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
 * --------------------------------------------------
 * Request → ThreadPool
 * --------------------------------------------------
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

    /*
     * TCP 数据还没完整。
     */
    if (
        result.status ==
        ParseStatus::NeedMoreData
    )
    {
        return true;
    }

    /*
     * Parser framing 已经失去可信度。
     *
     * 返回错误后必须关闭连接。
     */
    if (
        result.status ==
        ParseStatus::Error
    )
    {
        const int statusCode =
            result.errorStatusCode;

        const std::string reason =
            reasonPhraseForStatus(
                statusCode
            );

        Logger::warning(
            "HTTP parse error fd="
            + std::to_string(
                connection.fd
            )
            + " status="
            + std::to_string(
                statusCode
            )
            + " reason=\""
            + result.error
            + "\""
        );

        HttpResponse response =
            makeTextResponse(
                statusCode,
                reason,
                std::to_string(
                    statusCode
                )
                    + " "
                    + reason
                    + "\n"
            );

        setHeader(
            response,
            "Connection",
            "close"
        );

        return appendResponse(
            epollFd,
            connection,
            response,
            true
        );
    }

    /*
     * 已得到一个完整请求。
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

                const auto start =
                    std::chrono::
                        steady_clock::now();

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
                }
                catch (
                    const std::exception&
                    error
                )
                {
                    Logger::error(
                        std::string(
                            "handler exception: "
                        )
                        + error.what()
                    );

                    completion.response =
                        makeTextResponse(
                            500,
                            "Internal Server Error",
                            "500 Internal Server Error\n"
                        );

                    completion.closeAfterWrite =
                        true;
                }
                catch (...)
                {
                    Logger::error(
                        "unknown handler exception"
                    );

                    completion.response =
                        makeTextResponse(
                            500,
                            "Internal Server Error",
                            "500 Internal Server Error\n"
                        );

                    completion.closeAfterWrite =
                        true;
                }

                const auto end =
                    std::chrono::
                        steady_clock::now();

                const auto duration =
                    std::chrono::
                        duration_cast<
                            std::chrono::
                                milliseconds
                        >(
                            end - start
                        ).count();

                /*
                 * Access Log
                 */
                Logger::info(
                    task.request.method
                    + " "
                    + task.request.target
                    + " -> "
                    + std::to_string(
                        completion
                            .response
                            .statusCode
                    )
                    + " "
                    + std::to_string(
                        duration
                    )
                    + "ms"
                );

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
     */
    connection.processing =
        false;

    Logger::warning(
        "thread pool overloaded, fd="
        + std::to_string(
            connection.fd
        )
        + ", returning 503"
    );

    HttpResponse response =
        makeTextResponse(
            503,
            "Service Unavailable",
            "503 Service Unavailable\n"
        );

    setHeader(
        response,
        "Connection",
        "close"
    );

    return appendResponse(
        epollFd,
        connection,
        response,
        true
    );
}

/*
 * --------------------------------------------------
 * Worker completion
 * --------------------------------------------------
 */

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
         * Worker 执行期间客户端已经断开。
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
         * fd 已经被 Linux 复用。
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

        ++connection.requestCount;

        bool closeAfterWrite =
            completion.closeAfterWrite
            ||
            connection.peerClosed;

        /*
         * 每个 TCP connection 最多处理
         * MaxRequestsPerConnection 个请求。
         */
        if (
            connection.requestCount
            >=
            MaxRequestsPerConnection
        )
        {
            closeAfterWrite =
                true;
        }

        if (closeAfterWrite)
        {
            setHeader(
                completion.response,
                "Connection",
                "close"
            );

            eraseHeader(
                completion.response,
                "Keep-Alive"
            );
        }
        else
        {
            /*
             * HTTP/1.1 本来就默认 keep-alive。
             *
             * 这里显式输出，方便学习和调试。
             * 对 HTTP/1.0 也能明确保持连接。
             */
            setHeader(
                completion.response,
                "Connection",
                "keep-alive"
            );

            const std::size_t remaining =
                MaxRequestsPerConnection
                - connection.requestCount;

            setHeader(
                completion.response,
                "Keep-Alive",
                "timeout=60, max="
                    + std::to_string(
                        remaining
                    )
            );
        }

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
         * readBuffer 中可能已经存在下一个
         * HTTP Request。
         *
         * 不需要等待新的 EPOLLIN。
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

/*
 * --------------------------------------------------
 * Keep-Alive Idle Timeout
 * --------------------------------------------------
 */

void closeIdleConnections(
    int epollFd,
    std::unordered_map<
        int,
        Connection
    >& connections
)
{
    const auto now =
        std::chrono::
            steady_clock::now();

    std::vector<int> expired;

    for (
        const auto& [fd, connection] :
        connections
    )
    {
        /*
         * Worker 正在处理请求时，
         * 暂不做 idle timeout。
         */
        if (connection.processing)
        {
            continue;
        }

        if (
            now
            - connection.lastActivity
            >
            IdleTimeout
        )
        {
            expired.push_back(fd);
        }
    }

    for (int fd : expired)
    {
        Logger::info(
            "idle timeout fd="
            + std::to_string(fd)
        );

        disconnectConnection(
            epollFd,
            connections,
            fd
        );
    }
}

}

int main()
{
    /*
     * ------------------------------------------------
     * socket
     * ------------------------------------------------
     */

    const int serverFd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (serverFd == -1)
    {
        Logger::error(
            std::string(
                "socket failed: "
            )
            + std::strerror(errno)
        );

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
        Logger::error(
            std::string(
                "setsockopt failed: "
            )
            + std::strerror(errno)
        );

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
        Logger::error(
            std::string(
                "bind failed: "
            )
            + std::strerror(errno)
        );

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
        Logger::error(
            std::string(
                "listen failed: "
            )
            + std::strerror(errno)
        );

        close(serverFd);

        return 1;
    }

    if (
        !setNonBlocking(
            serverFd
        )
    )
    {
        Logger::error(
            "failed to set serverFd non-blocking"
        );

        close(serverFd);

        return 1;
    }

    /*
     * ------------------------------------------------
     * epoll
     * ------------------------------------------------
     */

    const int epollFd =
        epoll_create1(
            EPOLL_CLOEXEC
        );

    if (epollFd == -1)
    {
        Logger::error(
            std::string(
                "epoll_create1 failed: "
            )
            + std::strerror(errno)
        );

        close(serverFd);

        return 1;
    }

    /*
     * Worker → Event Loop
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
        Logger::error(
            std::string(
                "eventfd failed: "
            )
            + std::strerror(errno)
        );

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
        Logger::error(
            "failed to register serverFd"
        );

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
        Logger::error(
            "failed to register wakeFd"
        );

        close(wakeFd);
        close(epollFd);
        close(serverFd);

        return 1;
    }

    /*
     * ------------------------------------------------
     * HTTP Components
     * ------------------------------------------------
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
     * 注册完成以后 Router 不再修改。
     * Worker 只做并发只读。
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

    const std::size_t workerCount =
        std::max(
            1u,
            hardwareThreads
        );

    ThreadPool threadPool(
        workerCount,
        MaxPendingTasks
    );

    Logger::info(
        "worker threads="
        + std::to_string(
            workerCount
        )
    );

    /*
     * ------------------------------------------------
     * Connections
     * ------------------------------------------------
     */

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

    Logger::info(
        "HTTP server listening on 0.0.0.0:8080"
    );

    while (true)
    {
        /*
         * 不再无限等待。
         *
         * 至少每 1 秒醒来一次，
         * 用于检查 idle timeout。
         */
        const int eventCount =
            epoll_wait(
                epollFd,
                events,
                MaxEvents,
                EpollWaitTimeoutMs
            );

        if (eventCount == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            Logger::error(
                std::string(
                    "epoll_wait failed: "
                )
                + std::strerror(errno)
            );

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
             * ----------------------------------------
             * accept
             * ----------------------------------------
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

                        Connection connection;

                        connection.fd =
                            clientFd;

                        connection.id =
                            nextConnectionId++;

                        connection.lastActivity =
                            std::chrono::
                                steady_clock::now();

                        const std::uint64_t
                            newConnectionId =
                                connection.id;

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

                        Logger::info(
                            "client connected fd="
                            + std::to_string(
                                clientFd
                            )
                            + " connectionId="
                            + std::to_string(
                                newConnectionId
                            )
                        );

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

                    Logger::error(
                        std::string(
                            "accept failed: "
                        )
                        + std::strerror(
                            errno
                        )
                    );

                    break;
                }

                continue;
            }

            /*
             * ----------------------------------------
             * Worker completion event
             * ----------------------------------------
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
             * Fatal socket state。
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
             * ----------------------------------------
             * Read
             * ----------------------------------------
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
                 * processing == true：
                 * 不会提交第二个 Request，
                 * 但 recv 数据仍然允许积存在 readBuffer。
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
                 * peer 已关闭发送方向后，
                 * 不再继续关注 EPOLLIN/RDHUP。
                 */
                if (
                    connections.find(fd)
                    != connections.end()
                )
                {
                    if (
                        !updateConnectionEvents(
                            epollFd,
                            connection
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

                /*
                 * 对端不会再发送任何数据，
                 * 又没有请求正在处理，
                 * 也没有 Response 等待发送：
                 *
                 * 直接清理。
                 */
                if (
                    connection.peerClosed
                    &&
                    !connection.processing
                    &&
                    connection
                        .writeBuffer
                        .empty()
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
             * 上面可能已经 erase。
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

            /*
             * ----------------------------------------
             * Write
             * ----------------------------------------
             */

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

        /*
         * 每次 event-loop iteration
         * 完成后检查 Keep-Alive idle timeout。
         */
        closeIdleConnections(
            epollFd,
            connections
        );
    }

    /*
     * 先停 Worker。
     *
     * 否则还有 Worker 可能 write(wakeFd)，
     * 但我们已经把 wakeFd close。
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