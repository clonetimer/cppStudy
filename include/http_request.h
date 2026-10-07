#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <unordered_map>

struct HttpRequest
{
    std::string method;

    std::string target;

    std::string version;

    std::unordered_map<
        std::string,
        std::string
    > headers;

    std::string body;
};

#endif