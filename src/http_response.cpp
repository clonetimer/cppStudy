#include "http_response.h"

std::string serializeResponse(
    const HttpResponse& response
)
{
    std::string output;

    output.reserve(
        response.body.size()
        + 256
    );

    output +=
        "HTTP/1.1 ";

    output +=
        std::to_string(
            response.statusCode
        );

    output += ' ';

    output +=
        response.reasonPhrase;

    output +=
        "\r\n";

    for (
        const auto& [name, value] :
        response.headers
    )
    {
        /*
         * Content-Length 由 serializer
         * 统一负责。
         */
        if (
            name ==
            "Content-Length"
        )
        {
            continue;
        }

        output += name;
        output += ": ";
        output += value;
        output += "\r\n";
    }

    output +=
        "Content-Length: ";

    output +=
        std::to_string(
            response.body.size()
        );

    output +=
        "\r\n";

    output +=
        "\r\n";

    output +=
        response.body;

    return output;
}