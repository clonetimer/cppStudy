#ifndef HTTP_REQUEST_PARSE_H
#define HTTP_REQUEST_PARSE_H

#include "http_request.h"

#include <cstddef>
#include <string>
#include <string_view>

enum class ParseStatus
{
    NeedMoreData,
    Complete,
    Error
};

struct ParseResult
{
    ParseStatus status = ParseStatus::NeedMoreData;

    HttpRequest request;

    std::size_t consumed = 0;

    std::string error;
};

class HttpRequestParser
{
public:
    ParseResult parse(
        std::string_view data
    ) const;
};


#endif // HTTP_REQUEST_PARSE_H