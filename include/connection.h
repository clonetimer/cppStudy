#ifndef CONNECTION_H
#define CONNECTION_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

struct Connection
{
    int fd = -1;

    /*
     * fd 会被操作系统复用。
     *
     * connectionId 用来区分：
     *
     * old fd=8 id=100
     * new fd=8 id=101
     */
    std::uint64_t id = 0;

    std::string readBuffer;

    std::string writeBuffer;

    /*
     * 当前是否已有一个 HTTP Request
     * 在线程池中处理。
     */
    bool processing = false;

    /*
     * writeBuffer 全部发送完成以后关闭连接。
     */
    bool closeAfterWrite = false;

    /*
     * 对端已经关闭发送方向。
     */
    bool peerClosed = false;

    /*
     * 当前 TCP connection 已完成多少个 HTTP 请求。
     */
    std::size_t requestCount = 0;

    /*
     * 最近一次真正发生网络 I/O 的时间。
     *
     * timeout 必须使用 steady_clock，
     * 避免系统时间调整影响超时判断。
     */
    std::chrono::steady_clock::time_point
        lastActivity =
            std::chrono::steady_clock::now();
};

#endif