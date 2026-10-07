#include "http_request_parser.h"
#include "http_response.h"
#include "static_file_handler.h"
#include "router.h"
#include "api_handlers.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace
{

bool sendAll(
    int fd,
    const std::string& data
)
{
    std::size_t offset = 0;

    while (offset < data.size())
    {
        const ssize_t sent =
            send(
                fd,
                data.data() + offset,
                data.size() - offset,
                MSG_NOSIGNAL
            );

        if (sent > 0)
        {
            offset +=
                static_cast<std::size_t>(sent);

            continue;
        }

        if (
            sent == -1 &&
            errno == EINTR
        )
        {
            continue;
        }

        return false;
    }

    return true;
}

HttpResponse dispatchRequest(
    const HttpRequest& request,
    const Router& router,
    const StaticFileHandler& staticFiles
)
{
    std::string path =
        request.target;

    const std::size_t query =
        path.find('?');

    if (
        query !=
        std::string::npos
    )
    {
        path.erase(query);
    }

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

void handleClient(
    int clientFd,
    const Router& router,
    const StaticFileHandler& staticFiles
)
{
    HttpRequestParser parser;
    std::string readBuffer;
    char buffer[4096];

    while (true)
    {
        const ssize_t received =
            recv(clientFd, buffer, sizeof(buffer), 0);

        if (received > 0)
        {
            readBuffer.append(
                buffer,
                static_cast<std::size_t>(received)
            );

            const ParseResult result =
                parser.parse(readBuffer);

            if (result.status == ParseStatus::NeedMoreData)
            {
                continue;
            }

            if (result.status == ParseStatus::Error)
            {
                HttpResponse response;
                response.statusCode = 400;
                response.reasonPhrase = "Bad Request";
                response.headers["Content-Type"] = "text/plain; charset=utf-8";
                response.headers["Connection"] = "close";
                response.body = "400 Bad Request\n";

                // ✅ 只序列化一次，检查发送结果
                const std::string responseData =
                    serializeResponse(response);

                if (!sendAll(clientFd, responseData))
                {
                    std::cerr
                        << "send failed (400 response): "
                        << std::strerror(errno)
                        << '\n';
                }

                return;
            }

            // ✅ 正常请求处理
            HttpResponse response =
                dispatchRequest(
                    result.request,
                    router,
                    staticFiles
                );

            // 消费已解析的数据
            readBuffer.erase(
                0,
                result.consumed
            );

            // 加上 Connection: close 头
            response.headers["Connection"] = "close";

            // ✅ 只序列化一次，存到变量里
            const std::string responseData =
                serializeResponse(response);

            // ✅ 检查 sendAll 返回值，失败则记录错误
            if (!sendAll(clientFd, responseData))
            {
                std::cerr
                    << "send failed (response): "
                    << std::strerror(errno)
                    << '\n';
            }

            return;
        }

        if (received == 0)
        {
            // 客户端正常关闭连接
            return;
        }

        // received == -1
        if (errno == EINTR)
        {
            continue;
        }

        // ✅ recv 失败：记录错误后返回（连接已不可用）
        std::cerr
            << "recv failed: "
            << std::strerror(errno)
            << '\n';

        return;
    }
}



}

int main()
{
    constexpr int Port = 8080;

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
        htons(Port);

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

    /*
     * 注意：
     * 这里的路径是相对于你“启动程序时的当前目录”。
     *
     * 如果你在项目根目录运行：
     *
     * ./build/http_server
     *
     * 那么 "www" 正好对应：
     *
     * 项目根目录/www
     */
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

    const StaticFileHandler staticFiles(
        "www"
    );

    std::cout
        << "HTTP server listening on "
        << "http://127.0.0.1:"
        << Port
        << '\n';

    while (true)
    {
        const int clientFd =
            accept(
                serverFd,
                nullptr,
                nullptr
            );

        if (clientFd == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr
                << "accept failed: "
                << std::strerror(errno)
                << '\n';

            break;
        }

        handleClient(
            clientFd,
            router,
            staticFiles
        );

        close(clientFd);
    }

    close(serverFd);

    return 0;
}