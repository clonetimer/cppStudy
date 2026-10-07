#include "router.h"

#include <gtest/gtest.h>

TEST(
    RouterTest,
    RoutesMatchingRequest
)
{
    Router router;

    router.addRoute(
        "GET",
        "/hello",
        [](
            const HttpRequest&
        )
        {
            HttpResponse response;

            response.statusCode = 200;
            response.body = "hello";

            return response;
        }
    );

    HttpRequest request;

    request.method = "GET";
    request.target = "/hello";

    HttpResponse response =
        router.handle(
            request
        );

    EXPECT_EQ(
        response.statusCode,
        200
    );

    EXPECT_EQ(
        response.body,
        "hello"
    );
}

TEST(
    RouterTest,
    MethodIsPartOfRoute
)
{
    Router router;

    router.addRoute(
        "GET",
        "/hello",
        [](
            const HttpRequest&
        )
        {
            HttpResponse response;

            response.statusCode = 200;

            return response;
        }
    );

    EXPECT_FALSE(
        router.hasRoute(
            "POST",
            "/hello"
        )
    );
}

TEST(
    RouterTest,
    IgnoresQueryStringForMatching
)
{
    Router router;

    router.addRoute(
        "GET",
        "/hello",
        [](
            const HttpRequest&
        )
        {
            HttpResponse response;

            response.statusCode = 200;

            return response;
        }
    );

    HttpRequest request;

    request.method = "GET";

    request.target =
        "/hello?name=Alice";

    HttpResponse response =
        router.handle(
            request
        );

    EXPECT_EQ(
        response.statusCode,
        200
    );
}