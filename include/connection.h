#ifndef CONNECTION_H
#define CONNECTION_H

#include <cstdint>
#include <string>

struct Connection
{
    int fd = -1;

    /*
     * fd 会被 Linux 复用。
     *
     * connectionId 用来区分：
     *
     * old fd=8 id=100
     * new fd=8 id=101
     *
     * 防止旧 Worker 的结果写给新连接。
     */
    std::uint64_t id = 0;

    std::string readBuffer;

    std::string writeBuffer;

    /*
     * 当前是否已经有一个 HTTP Request
     * 在线程池中处理。
     *
     * D5 暂时规定：
     * 每个 connection 最多一个 in-flight request。
     */
    bool processing = false;

    /*
     * Response 全部发送完成以后关闭连接。
     */
    bool closeAfterWrite = false;

    /*
     * 对端已经关闭发送方向。
     */
    bool peerClosed = false;
};

#endif