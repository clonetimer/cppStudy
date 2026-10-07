#include "api_handlers.h"

HttpResponse helloHandler(
    const HttpRequest&
)
{
    HttpResponse response;

    response.statusCode = 200;
    response.reasonPhrase = "OK";

    response.headers[
        "Content-Type"
    ] =
        "text/plain; charset=utf-8";

    response.body =
        "Hello from C++ HTTP Server!\n";

    return response;
}

HttpResponse echoHandler(
    const HttpRequest& request
)
{
    HttpResponse response;

    response.statusCode = 200;
    response.reasonPhrase = "OK";

    response.headers[
        "Content-Type"
    ] =
        "text/plain; charset=utf-8";

    response.body =
        request.body;

    return response;
}

HttpResponse healthHandler(
    const HttpRequest&
)
{
    HttpResponse response;

    response.statusCode = 200;
    response.reasonPhrase = "OK";

    response.headers[
        "Content-Type"
    ] =
        "application/json";

    response.body =
        R"({"status":"ok"})";

    return response;
}