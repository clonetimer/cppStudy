#include "chat_protocol.h"

#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace
{

bool sendAll(
    int fd,
    std::string_view data
)
{
    std::size_t offset = 0;

    while (offset < data.size())
    {
        const ssize_t sent =
            send(
                fd,
                data.data() + offset,
                data.size() - offset,
                MSG_NOSIGNAL
            );

        if (sent > 0)
        {
            offset +=
                static_cast<std::size_t>(
                    sent
                );

            continue;
        }

        if (
            sent == -1 &&
            errno == EINTR
        )
        {
            continue;
        }

        return false;
    }

    return true;
}

bool receiveLine(
    int fd,
    std::string& buffer,
    std::string& line
)
{
    while (true)
    {
        const std::size_t position =
            buffer.find('\n');

        if (
            position !=
            std::string::npos
        )
        {
            line =
                buffer.substr(
                    0,
                    position
                );

            buffer.erase(
                0,
                position + 1
            );

            if (
                !line.empty() &&
                line.back() == '\r'
            )
            {
                line.pop_back();
            }

            return true;
        }

        char temporary[4096];

        const ssize_t received =
            recv(
                fd,
                temporary,
                sizeof(temporary),
                0
            );

        if (received > 0)
        {
            const auto size =
                static_cast<std::size_t>(
                    received
                );

            if (
                size >
                    chat::MaxReadBufferSize
                ||
                buffer.size()
                    >
                    chat::MaxReadBufferSize
                        - size
            )
            {
                return false;
            }

            buffer.append(
                temporary,
                size
            );

            continue;
        }

        if (received == 0)
        {
            return false;
        }

        if (errno == EINTR)
        {
            continue;
        }

        return false;
    }
}

}

int main()
{
    const int fd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (fd == -1)
    {
        std::cerr
            << "socket failed: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    sockaddr_in address{};

    address.sin_family =
        AF_INET;

    address.sin_port =
        htons(8080);

    if (
        inet_pton(
            AF_INET,
            "127.0.0.1",
            &address.sin_addr
        ) != 1
    )
    {
        std::cerr
            << "invalid server address\n";

        close(fd);

        return 1;
    }

    if (
        connect(
            fd,
            reinterpret_cast<
                sockaddr*
            >(&address),
            sizeof(address)
        ) == -1
    )
    {
        std::cerr
            << "connect failed: "
            << std::strerror(errno)
            << '\n';

        close(fd);

        return 1;
    }

    std::cout
        << "Connected to "
        << "127.0.0.1:8080\n";

    /*
     * handshakeBuffer 可能一次 recv()
     * 收到：
     *
     * OK welcome Alice\n
     * *** Bob joined ***\n
     *
     * receiveLine 只拿走第一行，
     * 剩余数据继续留在 buffer。
     */
    std::string handshakeBuffer;

    while (true)
    {
        std::string nickname;

        std::cout
            << "Nickname: ";

        if (
            !std::getline(
                std::cin,
                nickname
            )
        )
        {
            close(fd);

            return 0;
        }

        if (
            !chat::isValidNickname(
                nickname
            )
        )
        {
            std::cerr
                << "Invalid nickname.\n"
                << "Allowed: letters, numbers, "
                << "'_' and '-'. "
                << "Maximum "
                << chat::MaxNicknameLength
                << " characters.\n";

            continue;
        }

        const std::string request =
            "NICK "
            + nickname
            + "\n";

        if (
            !sendAll(
                fd,
                request
            )
        )
        {
            std::cerr
                << "failed to send nickname\n";

            close(fd);

            return 1;
        }

        std::string response;

        if (
            !receiveLine(
                fd,
                handshakeBuffer,
                response
            )
        )
        {
            std::cerr
                << "server disconnected "
                << "during registration\n";

            close(fd);

            return 1;
        }

        std::cout
            << response
            << '\n';

        if (
            response.rfind(
                "OK ",
                0
            ) == 0
        )
        {
            break;
        }
    }

    std::atomic<bool> running{
        true
    };

    /*
     * 把 handshake 阶段 recv 到但尚未消费的
     * 字节交给 receiver。
     */
    std::thread receiver(
        [
            fd,
            &running,
            buffer =
                std::move(
                    handshakeBuffer
                )
        ]() mutable
        {
            auto printCompleteLines =
                [&buffer]()
                {
                    auto lines =
                        chat::extractLines(
                            buffer
                        );

                    for (
                        const auto& line :
                        lines
                    )
                    {
                        std::cout
                            << line
                            << '\n';
                    }

                    std::cout.flush();
                };

            printCompleteLines();

            char temporary[4096];

            while (
                running.load()
            )
            {
                const ssize_t received =
                    recv(
                        fd,
                        temporary,
                        sizeof(temporary),
                        0
                    );

                if (received > 0)
                {
                    buffer.append(
                        temporary,
                        static_cast<
                            std::size_t
                        >(received)
                    );

                    printCompleteLines();

                    continue;
                }

                if (received == 0)
                {
                    break;
                }

                if (errno == EINTR)
                {
                    continue;
                }

                /*
                 * shutdown() from main may cause
                 * recv() to fail while we're exiting.
                 */
                if (running.load())
                {
                    std::cerr
                        << "recv failed: "
                        << std::strerror(
                            errno
                        )
                        << '\n';
                }

                break;
            }

            running.store(false);
        }
    );

    std::string line;

    while (
        running.load()
        &&
        std::getline(
            std::cin,
            line
        )
    )
    {
        if (
            line.size()
            > chat::MaxMessageLength
        )
        {
            std::cerr
                << "Message too long. "
                << "Maximum "
                << chat::MaxMessageLength
                << " bytes.\n";

            continue;
        }

        if (
            !sendAll(
                fd,
                line + "\n"
            )
        )
        {
            std::cerr
                << "send failed\n";

            break;
        }

        if (line == "/quit")
        {
            break;
        }
    }

    running.store(false);

    /*
     * 先 shutdown，让另一个线程里阻塞的
     * recv() 返回。
     *
     * 不要先 close 再 join，
     * 避免 fd 在 receiver 尚未退出时被系统复用。
     */
    shutdown(
        fd,
        SHUT_RDWR
    );

    if (
        receiver.joinable()
    )
    {
        receiver.join();
    }

    close(fd);

    return 0;
}