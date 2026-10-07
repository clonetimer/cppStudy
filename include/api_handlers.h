#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include "http_request.h"
#include "http_response.h"

HttpResponse helloHandler(
    const HttpRequest& request
);

HttpResponse echoHandler(
    const HttpRequest& request
);

HttpResponse healthHandler(
    const HttpRequest& request
);

#endif