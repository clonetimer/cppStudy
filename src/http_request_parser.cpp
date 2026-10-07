#include "http_request_parser.h"

#include <charconv>
#include <cctype>
#include <sstream>
#include <string>
#include <utility>

namespace
{

    constexpr std::size_t MaxHeaderSize =
        16 * 1024;

    constexpr std::size_t MaxBodySize =
        1024 * 1024;


    // 找出非空白字符的首尾位置，并返回去掉首尾空白字符的字符串。    
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

    // 将字符串转换为小写。
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

    // 解析请求行，返回是否成功。
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


    // 解析 Header 行，返回是否成功。
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

        if (
            request.headers.find(name)
            != request.headers.end()
        )
        {
            return false;
        }
        // 简化策略：根据HTTP/1.1 规范要求同名 Header 不允许重复。
        request.headers.emplace(
            std::move(name),
            std::move(value)
        );

        return true;
    }

    // 解析 Content-Length，返回是否成功。
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
     * 第一步：
     * 等待完整 Header，判断是否超出最大Header值。
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
            result.status =
                ParseStatus::Error;

            result.error =
                "HTTP header too large";
        }

        return result;
    }

    const std::size_t headerBytes =
        headerEnd + 4;

    if (
        headerBytes >
        MaxHeaderSize
    )
    {
        result.status =
            ParseStatus::Error;

        result.error =
            "HTTP header too large";

        return result;
    }

    /*
     * 第二步：
     * Request Line。
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
        result.status =
            ParseStatus::Error;

        result.error =
            "invalid request line";

        return result;
    }

    const std::string_view requestLine =
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
        result.status =
            ParseStatus::Error;

        result.error =
            "invalid request line";

        return result;
    }

    /*
     * 第三步：
     * Headers。
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
            result.status =
                ParseStatus::Error;

            result.error =
                "invalid header";

            return result;
        }

        const std::string_view line =
            buffer.substr(
                position,
                lineEnd - position
            );

        if (line.empty())
        {
            result.status =
                ParseStatus::Error;

            result.error =
                "unexpected empty header";

            return result;
        }

        if (
            !parseHeaderLine(
                line,
                result.request
            )
        )
        {
            result.status =
                ParseStatus::Error;

            result.error =
                "invalid or duplicate header";

            return result;
        }

        position =
            lineEnd + 2;
    }

    /*
     * HTTP/1.1 学习版：
     * 要求 Host。
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
        result.status =
            ParseStatus::Error;

        result.error =
            "missing Host header";

        return result;
    }

    /*
     * 这个 Parser 暂不支持
     * Transfer-Encoding。
     */
    if (
        result.request.headers.find(
            "transfer-encoding"
        )
        !=
        result.request.headers.end()
    )
    {
        result.status =
            ParseStatus::Error;

        result.error =
            "Transfer-Encoding is not supported";

        return result;
    }

    /*
     * 第四步：
     * Content-Length。
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
            result.status =
                ParseStatus::Error;

            result.error =
                "invalid Content-Length";

            return result;
        }

        if (
            contentLength >
            MaxBodySize
        )
        {
            result.status =
                ParseStatus::Error;

            result.error =
                "HTTP body too large";

            return result;
        }
    }

    /*
     * 防止 size_t 加法溢出。
     */
    if (
        contentLength >
        static_cast<std::size_t>(-1)
            - headerBytes
    )
    {
        result.status =
            ParseStatus::Error;

        result.error =
            "invalid message size";

        return result;
    }

    const std::size_t totalBytes =
        headerBytes
        + contentLength;

    /*
     * Header 已经完整，
     * 但 Body 还没完整。
     */
    if (
        buffer.size() <
        totalBytes
    )
    {
        result.status =
            ParseStatus::NeedMoreData;

        return result;
    }

    /*
     * 第五步：
     * Body 完整。
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