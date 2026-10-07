#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "http_request.h"

#include <cstddef>
#include <string>
#include <string_view>


struct HttpResponse
{
    int statusCode = 200;

    std::string reasonPhrase =
        "OK";

    std::unordered_map<
        std::string,
        std::string
    > headers;

    std::string body;
};