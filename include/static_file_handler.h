#ifndef STATIC_FILE_HANDLER_H
#define STATIC_FILE_HANDLER_H

#include "http_request.h"
#include "http_response.h"

#include <filesystem>

class StaticFileHandler
{
public:
    explicit StaticFileHandler(
        std::filesystem::path documentRoot
    );

    HttpResponse handle(
        const HttpRequest& request
    ) const;

private:
    std::filesystem::path
        documentRoot_;
};

#endif