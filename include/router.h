#ifndef ROUTER_H
#define ROUTER_H

#include "http_request.h"
#include "http_response.h"

#include <functional>
#include <string>
#include <unordered_map>

class Router
{
public:
    using Handler =
        std::function<
            HttpResponse(
                const HttpRequest&
            )
        >;

    void addRoute(
        std::string method,
        std::string path,
        Handler handler
    );

    bool hasRoute(
        const std::string& method,
        const std::string& path
    ) const;

    bool hasPath(
        const std::string& path
    ) const;

    HttpResponse handle(
        const HttpRequest& request
    ) const;

private:
    static std::string makeKey(
        const std::string& method,
        const std::string& path
    );

    std::unordered_map<
        std::string,
        Handler
    > routes_;
};

#endif