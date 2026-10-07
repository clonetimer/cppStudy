#include "static_file_handler.h"
#include "gtest/gtest.h"
#include <string>

TEST(
    StaticFileHandlerTest,
    RejectsNonGet
)
{
    StaticFileHandler handler(
        "www"
    );

    HttpRequest request;

    request.method = "POST";
    request.target = "/";

    HttpResponse response =
        handler.handle(request);

    EXPECT_EQ(
        response.statusCode,
        405
    );
}