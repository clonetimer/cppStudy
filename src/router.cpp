#include "router.h"

#include <utility>

namespace
{

std::string extractPath(
    const std::string& target
)
{
    const std::size_t queryPosition =
        target.find('?');

    if (
        queryPosition ==
        std::string::npos
    )
    {
        return target;
    }

    return target.substr(
        0,
        queryPosition
    );
}

}

void Router::addRoute(
    std::string method,
    std::string path,
    Handler handler
)
{
    routes_[
        makeKey(
            method,
            path
        )
    ] = std::move(handler);
}

bool Router::hasRoute(
    const std::string& method,
    const std::string& path
) const
{
    return routes_.find(
        makeKey(
            method,
            path
        )
    ) != routes_.end();
}

HttpResponse Router::handle(
    const HttpRequest& request
) const
{
    const std::string path =
        extractPath(
            request.target
        );

    const auto it =
        routes_.find(
            makeKey(
                request.method,
                path
            )
        );

    if (
        it ==
        routes_.end()
    )
    {
        HttpResponse response;

        response.statusCode = 404;
        response.reasonPhrase =
            "Not Found";

        response.headers[
            "Content-Type"
        ] =
            "text/plain; charset=utf-8";

        response.body =
            "404 Not Found\n";

        return response;
    }

    return it->second(
        request
    );
}

bool Router::hasPath(
    const std::string& path
) const
{
    for (
        const auto& [key, handler] :
        routes_
    )
    {
        (void)handler;

        const std::size_t space =
            key.find(' ');

        if (
            space !=
            std::string::npos
            &&
            key.substr(
                space + 1
            ) == path
        )
        {
            return true;
        }
    }

    return false;
}

std::string Router::makeKey(
    const std::string& method,
    const std::string& path
)
{
    return method
        + " "
        + path;
}