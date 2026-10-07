#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <unordered_map>



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

std::string serializeResponse
(
    const HttpResponse& response
);

#endif