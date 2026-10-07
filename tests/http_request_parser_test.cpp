#include "http_request_parser.h"
#include "gtest/gtest.h"
#include <string>


// 测试 GET 请求的解析
TEST(
    HttpRequestParserTest,
    ParsesSimpleGet
)
{
    HttpRequestParser parser;

    const std::string data =
        "GET /hello HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    auto result =
        parser.parse(data);

    ASSERT_EQ(
        result.status,
        ParseStatus::Complete
    );

    EXPECT_EQ(
        result.request.method,
        "GET"
    );

    EXPECT_EQ(
        result.request.target,
        "/hello"
    );

    EXPECT_EQ(
        result.request.version,
        "HTTP/1.1"
    );

    EXPECT_TRUE(
        result.request.body.empty()
    );

    EXPECT_EQ(
        result.consumed,
        data.size()
    );
}

// 测试 POST+BODY 请求的解析
TEST(
    HttpRequestParserTest,
    ParsesBodyUsingContentLength
)
{
    HttpRequestParser parser;

    const std::string data =
        "POST /echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";

    auto result =
        parser.parse(data);

    ASSERT_EQ(
        result.status,
        ParseStatus::Complete
    );

    EXPECT_EQ(
        result.request.method,
        "POST"
    );

    EXPECT_EQ(
        result.request.body,
        "hello"
    );
}

// 测试Header分段
TEST(
    HttpRequestParserTest,
    NeedsMoreDataForPartialHeader
)
{
    HttpRequestParser parser;

    const std::string data =
        "GET /hello HTTP/1.1\r\n"
        "Host: local";

    auto result =
        parser.parse(data);

    EXPECT_EQ(
        result.status,
        ParseStatus::NeedMoreData
    );
}

// 测试BODY分段
TEST(
    HttpRequestParserTest,
    NeedsMoreDataForPartialBody
)
{
    HttpRequestParser parser;

    const std::string data =
        "POST /echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "he";

    auto result =
        parser.parse(data);

    EXPECT_EQ(
        result.status,
        ParseStatus::NeedMoreData
    );
}

// 测试粘在一起的两个请求
TEST(
    HttpRequestParserTest,
    ReportsConsumedBytes
)
{
    HttpRequestParser parser;

    const std::string first =
        "GET /a HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    const std::string second =
        "GET /b HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    const std::string data =
        first + second;

    auto result =
        parser.parse(data);

    ASSERT_EQ(
        result.status,
        ParseStatus::Complete
    );

    EXPECT_EQ(
        result.request.target,
        "/a"
    );

    EXPECT_EQ(
        result.consumed,
        first.size()
    );
}

// 测试错误请求:Host缺失
TEST(
    HttpRequestParserTest,
    RejectsHttp11WithoutHost
)
{
    HttpRequestParser parser;

    const std::string data =
        "GET / HTTP/1.1\r\n"
        "\r\n";

    auto result =
        parser.parse(data);

    EXPECT_EQ(
        result.status,
        ParseStatus::Error
    );
}

// 测试错误请求:Content-Length缺失
TEST(
    HttpRequestParserTest,
    RejectsInvalidContentLength
)
{
    HttpRequestParser parser;

    const std::string data =
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: abc\r\n"
        "\r\n";

    auto result =
        parser.parse(data);

    EXPECT_EQ(
        result.status,
        ParseStatus::Error
    );
}