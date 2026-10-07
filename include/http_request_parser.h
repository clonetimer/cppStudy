#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H

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
    ParseStatus status =
        ParseStatus::NeedMoreData;

    HttpRequest request;

    std::size_t consumed = 0;

    /*
     * Parser 出错时建议返回的 HTTP 状态码。
     */
    int errorStatusCode = 400;

    std::string error;
};

class HttpRequestParser
{
public:
    ParseResult parse(
        std::string_view buffer
    ) const;
};

#endif