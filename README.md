# CppHttpServer

A lightweight HTTP/1.0 and HTTP/1.1 server written in C++17 for Linux.

The project was built from first principles to demonstrate TCP networking, non-blocking I/O, `epoll`, HTTP parsing, routing, static file serving, thread pools, persistent connections, resource limits, error handling, and concurrent server architecture.

## Features

- Linux non-blocking TCP sockets
- `epoll` event loop
- Worker thread pool
- `eventfd`-based worker-to-event-loop notification
- Incremental HTTP request parsing
- HTTP/1.0 and HTTP/1.1
- `Content-Length` request bodies
- HTTP Keep-Alive
- Idle connection timeout
- Maximum requests per connection
- Dynamic routing
- Static file serving
- MIME type detection
- Path traversal protection
- Partial read/write handling
- Request and response buffering
- Bounded worker task queue
- Thread-safe logging
- Access logging
- HTTP error responses
- GoogleTest/CTest support
- ASan/UBSan support

## Architecture

```text
                     Clients
                        |
                        v
                  +-----------+
                  |   epoll   |
                  | EventLoop |
                  +-----+-----+
                        |
             +----------+----------+
             |                     |
           recv                  EPOLLOUT
             |
             v
       readBuffer
             |
             v
    HttpRequestParser
             |
             v
       HttpRequest
             |
             v
       ThreadPool
      /          \
 Router       Static Files
      \          /
       HttpResponse
             |
             v
     CompletionQueue
             |
          eventfd
             |
             v
       Event Loop
             |
             v
       writeBuffer
             |
             v
           send
```

The event-loop thread owns socket and connection state.

Worker threads do not call `recv()`, `send()`, `close()`, or `epoll_ctl()`. They transform an `HttpRequest` into an `HttpResponse` and return the result through the completion queue.

## Supported Endpoints

### GET `/hello`

Returns a simple dynamically generated response.

```bash
curl -v http://127.0.0.1:8080/hello
```

### GET `/health`

Health endpoint.

```bash
curl -v http://127.0.0.1:8080/health
```

Example body:

```json
{"status":"ok"}
```

### POST `/echo`

Returns the request body.

```bash
curl \
    -v \
    -X POST \
    --data 'hello' \
    http://127.0.0.1:8080/echo
```

### Static Files

Files are served from the `www/` directory.

```text
GET /
    -> www/index.html

GET /style.css
    -> www/style.css
```

Example:

```bash
curl -v http://127.0.0.1:8080/
```

## HTTP Parsing

Requests are parsed incrementally.

The server does not assume that one TCP `recv()` corresponds to one HTTP request.

```text
TCP bytes
    |
    v
readBuffer
    |
    +-- incomplete header
    |       -> NeedMoreData
    |
    +-- incomplete body
    |       -> NeedMoreData
    |
    +-- invalid request
    |       -> Error
    |
    +-- complete request
            -> HttpRequest
```

The current implementation supports request bodies framed with `Content-Length`.

`Transfer-Encoding: chunked` is intentionally not supported.

## Error Handling

The server can return responses including:

| Status | Meaning |
|---:|---|
| 200 | OK |
| 400 | Bad Request |
| 404 | Not Found |
| 405 | Method Not Allowed |
| 413 | Payload Too Large |
| 431 | Request Header Fields Too Large |
| 500 | Internal Server Error |
| 503 | Service Unavailable |

Malformed HTTP framing causes the connection to be closed after the error response.

Internal exception details are written to the server log rather than exposed to clients.

## Keep-Alive

HTTP/1.1 connections are persistent by default.

```text
HTTP/1.1
Connection absent
    -> keep alive

Connection: close
    -> close after response
```

HTTP/1.0 connections close by default unless `Connection: keep-alive` is supplied.

The server also applies resource limits:

```text
Idle timeout:
60 seconds

Maximum requests per TCP connection:
100
```

These values are implementation policy rather than HTTP protocol requirements.

## Build

Requirements:

- Linux
- C++17-compatible GCC or Clang
- CMake 3.18 or later
- POSIX threads

Debug build:

```bash
cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel
```

Release build:

```bash
cmake \
    -S . \
    -B build-release \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF

cmake --build \
    build-release \
    --parallel
```

## Run

Run the server from the project root so the relative `www/` document root resolves correctly:

```bash
./build-release/http_server
```

Default listening address:

```text
0.0.0.0:8080
```

## Tests

Build with testing enabled:

```bash
cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel
```

Run:

```bash
ctest \
    --test-dir build \
    --output-on-failure
```

Tests cover HTTP request parsing, response serialization, routing, and static file handling.

## Sanitizers

```bash
cmake \
    -S . \
    -B build-sanitize \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_SANITIZERS=ON

cmake --build \
    build-sanitize \
    --parallel

ctest \
    --test-dir build-sanitize \
    --output-on-failure
```

## Benchmarking

Performance tests should use a Release build.

Warm up the server before recording results:

```bash
wrk \
    -t2 \
    -c32 \
    -d10s \
    http://127.0.0.1:8080/health
```

Example GET benchmark:

```bash
wrk \
    -t4 \
    -c64 \
    -d30s \
    --latency \
    http://127.0.0.1:8080/health
```

Static-file benchmark:

```bash
wrk \
    -t4 \
    -c64 \
    -d30s \
    --latency \
    http://127.0.0.1:8080/
```

Keep-Alive can also be tested with ApacheBench:

```bash
ab \
    -k \
    -n 10000 \
    -c 50 \
    http://127.0.0.1:8080/health
```

Benchmark results are machine- and configuration-dependent and should therefore be recorded together with build mode, CPU, concurrency, logging configuration, endpoint, duration, and error count.

### Benchmark Results

| Endpoint | Threads | Connections | Duration | Requests/sec | P99 | Errors |
|---|---:|---:|---:|---:|---:|---:|
| `/health` | 1 | 1 | 30s | TBD | TBD | TBD |
| `/health` | 4 | 16 | 30s | TBD | TBD | TBD |
| `/health` | 4 | 64 | 30s | TBD | TBD | TBD |
| `/health` | 4 | 128 | 30s | TBD | TBD | TBD |
| `/` | 4 | 64 | 30s | TBD | TBD | TBD |
| `POST /echo` | 4 | 64 | 30s | TBD | TBD | TBD |

Do not treat Debug-build results as representative performance numbers.

Per-request access logging also affects throughput and latency, so benchmark reports should state whether access logging was enabled.

## Current Limits

This is an educational HTTP server rather than a production replacement for nginx, Apache HTTP Server, or a mature application server.

Current limitations include:

- Linux only
- HTTP/2 is not supported
- HTTP/3 is not supported
- TLS/HTTPS is not implemented
- `Transfer-Encoding: chunked` is not implemented
- Request pipelining is processed sequentially per connection
- No URL percent-decoding layer
- Static file handling is intentionally simple
- No file cache
- No zero-copy `sendfile()`
- No graceful signal-based shutdown yet
- No production-grade structured logging or log rotation

## Design Principles

The project intentionally separates responsibilities:

```text
epoll/Event Loop
    -> network I/O and connection ownership

HttpRequestParser
    -> protocol parsing

Router
    -> dynamic route selection

StaticFileHandler
    -> static resource handling

ThreadPool
    -> controlled business concurrency

CompletionQueue + eventfd
    -> worker-to-event-loop communication

HttpResponse serializer
    -> response wire format
```

The central concurrency rule is:

> The event-loop thread owns sockets and connection state. Worker threads process request-to-response work without directly manipulating socket lifetime.

## License

Add the license appropriate for your repository.