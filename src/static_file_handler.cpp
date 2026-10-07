#include "static_file_handler.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{

std::optional<std::string> readFile(
    const std::filesystem::path& path
)
{
    std::error_code error;

    if (
        !std::filesystem::is_regular_file(
            path,
            error
        )
    )
    {
        return std::nullopt;
    }

    std::ifstream file(
        path,
        std::ios::binary
    );

    if (!file)
    {
        return std::nullopt;
    }

    std::ostringstream stream;

    stream << file.rdbuf();

    return stream.str();
}

std::string contentType(
    const std::filesystem::path& path
)
{
    const std::string extension =
        path.extension().string();

    if (extension == ".html")
    {
        return
            "text/html; charset=utf-8";
    }

    if (extension == ".css")
    {
        return
            "text/css; charset=utf-8";
    }

    if (extension == ".js")
    {
        return
            "application/javascript";
    }

    if (extension == ".txt")
    {
        return
            "text/plain; charset=utf-8";
    }

    if (extension == ".json")
    {
        return
            "application/json";
    }

    if (extension == ".png")
    {
        return "image/png";
    }

    if (
        extension == ".jpg"
        ||
        extension == ".jpeg"
    )
    {
        return "image/jpeg";
    }

    if (extension == ".svg")
    {
        return "image/svg+xml";
    }

    if (extension == ".ico")
    {
        return "image/x-icon";
    }

    return
        "application/octet-stream";
}

bool isSafeTarget(
    std::string_view target
)
{
    if (
        target.empty()
        ||
        target.front() != '/'
    )
    {
        return false;
    }

    /*
     * 学习版 path traversal 防御。
     *
     * 后续生产级版本还应：
     * URL decode + canonical path + symlink policy。
     */
    if (
        target.find("..")
        !=
        std::string_view::npos
    )
    {
        return false;
    }

    if (
        target.find('\\')
        !=
        std::string_view::npos
    )
    {
        return false;
    }

    return true;
}

HttpResponse textResponse(
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

}

StaticFileHandler::StaticFileHandler(
    std::filesystem::path documentRoot
)
    : documentRoot_(
        std::move(
            documentRoot
        )
    )
{
}

HttpResponse StaticFileHandler::handle(
    const HttpRequest& request
) const
{
    if (
        request.method != "GET"
    )
    {
        HttpResponse response =
            textResponse(
                405,
                "Method Not Allowed",
                "405 Method Not Allowed\n"
            );

        response.headers[
            "Allow"
        ] = "GET";

        return response;
    }

    if (
        !isSafeTarget(
            request.target
        )
    )
    {
        return textResponse(
            400,
            "Bad Request",
            "400 Bad Request\n"
        );
    }

    std::string target =
        request.target;

    /*
     * Query string 不是文件路径的一部分。
     */
    const std::size_t query =
        target.find('?');

    if (
        query !=
        std::string::npos
    )
    {
        target.erase(query);
    }

    if (target == "/")
    {
        target =
            "/index.html";
    }

    const std::filesystem::path relative =
        std::filesystem::path(
            target
        ).relative_path();

    const std::filesystem::path filePath =
        documentRoot_
        / relative;

    auto contents =
        readFile(
            filePath
        );

    if (!contents)
    {
        return textResponse(
            404,
            "Not Found",
            "404 Not Found\n"
        );
    }

    HttpResponse response;

    response.statusCode =
        200;

    response.reasonPhrase =
        "OK";

    response.headers[
        "Content-Type"
    ] =
        contentType(
            filePath
        );

    response.body =
        std::move(
            *contents
        );

    return response;
}