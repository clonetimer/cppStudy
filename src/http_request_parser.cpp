#include "http_request_parser.h"

#include <charconv>
#include <cctype>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace
{

constexpr std::size_t MaxHeaderSize =
    16 * 1024;

constexpr std::size_t MaxBodySize =
    1024 * 1024;

void setError(
    ParseResult& result,
    int statusCode,
    std::string message
)
{
    result.status =
        ParseStatus::Error;

    result.errorStatusCode =
        statusCode;

    result.error =
        std::move(message);
}

std::string trim(
    std::string_view value
)
{
    std::size_t begin = 0;

    while (
        begin < value.size()
        &&
        std::isspace(
            static_cast<unsigned char>(
                value[begin]
            )
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
        std::isspace(
            static_cast<unsigned char>(
                value[end - 1]
            )
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

std::string toLower(
    std::string_view text
)
{
    std::string result;

    result.reserve(
        text.size()
    );

    for (char ch : text)
    {
        result.push_back(
            static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(
                        ch
                    )
                )
            )
        );
    }

    return result;
}

bool parseRequestLine(
    std::string_view line,
    HttpRequest& request
)
{
    std::istringstream stream{
        std::string(line)
    };

    if (
        !(stream
          >> request.method
          >> request.target
          >> request.version)
    )
    {
        return false;
    }

    /*
     * 不允许：
     *
     * GET / HTTP/1.1 garbage
     */
    std::string extra;

    if (stream >> extra)
    {
        return false;
    }

    if (
        request.method.empty()
        ||
        request.target.empty()
    )
    {
        return false;
    }

    if (
        request.version != "HTTP/1.1"
        &&
        request.version != "HTTP/1.0"
    )
    {
        return false;
    }

    return true;
}

bool parseHeaderLine(
    std::string_view line,
    HttpRequest& request
)
{
    const std::size_t colon =
        line.find(':');

    if (
        colon ==
            std::string_view::npos
        ||
        colon == 0
    )
    {
        return false;
    }

    std::string name =
        toLower(
            trim(
                line.substr(
                    0,
                    colon
                )
            )
        );

    std::string value =
        trim(
            line.substr(
                colon + 1
            )
        );

    if (name.empty())
    {
        return false;
    }

    /*
     * 学习版采取保守策略：
     * 暂时拒绝重复 Header。
     *
     * 完整 HTTP 实现中，不同 Header
     * 对重复值有不同规则。
     */
    if (
        request.headers.find(name)
        != request.headers.end()
    )
    {
        return false;
    }

    request.headers.emplace(
        std::move(name),
        std::move(value)
    );

    return true;
}

bool parseContentLength(
    std::string_view text,
    std::size_t& result
)
{
    if (text.empty())
    {
        return false;
    }

    std::size_t value = 0;

    const auto [ptr, error] =
        std::from_chars(
            text.data(),
            text.data()
                + text.size(),
            value
        );

    if (
        error != std::errc{}
        ||
        ptr !=
            text.data()
                + text.size()
    )
    {
        return false;
    }

    result = value;

    return true;
}

}

ParseResult HttpRequestParser::parse(
    std::string_view buffer
) const
{
    ParseResult result;

    /*
     * ------------------------------------------------
     * 1. 等 Header 完整
     * ------------------------------------------------
     */

    const std::size_t headerEnd =
        buffer.find(
            "\r\n\r\n"
        );

    if (
        headerEnd ==
        std::string_view::npos
    )
    {
        if (
            buffer.size()
            > MaxHeaderSize
        )
        {
            setError(
                result,
                431,
                "HTTP header too large"
            );
        }

        return result;
    }

    /*
     * +4 对应：
     *
     * \r\n\r\n
     */
    if (
        headerEnd >
        MaxHeaderSize - 4
    )
    {
        setError(
            result,
            431,
            "HTTP header too large"
        );

        return result;
    }

    const std::size_t headerBytes =
        headerEnd + 4;

    /*
     * ------------------------------------------------
     * 2. Request Line
     * ------------------------------------------------
     */

    const std::size_t requestLineEnd =
        buffer.find("\r\n");

    if (
        requestLineEnd ==
            std::string_view::npos
        ||
        requestLineEnd >
            headerEnd
    )
    {
        setError(
            result,
            400,
            "invalid request line"
        );

        return result;
    }

    const std::string_view
        requestLine =
            buffer.substr(
                0,
                requestLineEnd
            );

    if (
        !parseRequestLine(
            requestLine,
            result.request
        )
    )
    {
        setError(
            result,
            400,
            "invalid request line"
        );

        return result;
    }

    /*
     * ------------------------------------------------
     * 3. Headers
     * ------------------------------------------------
     */

    std::size_t position =
        requestLineEnd + 2;

    while (position < headerEnd)
    {
        const std::size_t lineEnd =
            buffer.find(
                "\r\n",
                position
            );

        if (
            lineEnd ==
                std::string_view::npos
            ||
            lineEnd >
                headerEnd
        )
        {
            setError(
                result,
                400,
                "invalid header"
            );

            return result;
        }

        const std::string_view line =
            buffer.substr(
                position,
                lineEnd - position
            );

        if (line.empty())
        {
            setError(
                result,
                400,
                "unexpected empty header"
            );

            return result;
        }

        if (
            !parseHeaderLine(
                line,
                result.request
            )
        )
        {
            setError(
                result,
                400,
                "invalid or duplicate header"
            );

            return result;
        }

        position =
            lineEnd + 2;
    }

    /*
     * HTTP/1.1 要求 Host。
     */
    if (
        result.request.version ==
            "HTTP/1.1"
        &&
        result.request.headers.find(
            "host"
        )
        ==
        result.request.headers.end()
    )
    {
        setError(
            result,
            400,
            "missing Host header"
        );

        return result;
    }

    /*
     * D6 版本仍然暂不实现：
     *
     * Transfer-Encoding: chunked
     *
     * 为避免 framing 歧义，直接拒绝。
     */
    if (
        result.request.headers.find(
            "transfer-encoding"
        )
        !=
        result.request.headers.end()
    )
    {
        setError(
            result,
            400,
            "Transfer-Encoding is not supported"
        );

        return result;
    }

    /*
     * ------------------------------------------------
     * 4. Content-Length
     * ------------------------------------------------
     */

    std::size_t contentLength = 0;

    const auto contentLengthIt =
        result.request.headers.find(
            "content-length"
        );

    if (
        contentLengthIt !=
        result.request.headers.end()
    )
    {
        if (
            !parseContentLength(
                contentLengthIt->second,
                contentLength
            )
        )
        {
            setError(
                result,
                400,
                "invalid Content-Length"
            );

            return result;
        }

        if (
            contentLength >
            MaxBodySize
        )
        {
            setError(
                result,
                413,
                "HTTP body too large"
            );

            return result;
        }
    }

    /*
     * 防止 size_t 加法溢出。
     */
    if (
        contentLength >
        std::numeric_limits<
            std::size_t
        >::max()
            - headerBytes
    )
    {
        setError(
            result,
            400,
            "invalid HTTP message size"
        );

        return result;
    }

    const std::size_t totalBytes =
        headerBytes
        + contentLength;

    /*
     * Header 已完整，
     * Body 尚未全部到达。
     */
    if (
        buffer.size()
        < totalBytes
    )
    {
        return result;
    }

    /*
     * ------------------------------------------------
     * 5. Body
     * ------------------------------------------------
     */

    result.request.body =
        std::string(
            buffer.substr(
                headerBytes,
                contentLength
            )
        );

    result.consumed =
        totalBytes;

    result.status =
        ParseStatus::Complete;

    return result;
}