#include "http_response.h"
#include "gtest/gtest.h"
#include <string>

TEST(
    HttpResponseTest,
    AddsContentLength
)
{
    HttpResponse response;

    response.body = "hello";

    const std::string serialized =
        serializeResponse(response);

    EXPECT_NE(
        serialized.find(
            "Content-Length: 5\r\n"
        ),
        std::string::npos
    );
}