#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

bool sendAll(
    int fd,
    const std::string& data
)
{
    std::size_t offset = 0;

    while (offset < data.size())
    {
        ssize_t sent = send(
            fd,
            data.data() + offset,
            data.size() - offset,
            MSG_NOSIGNAL
        );

        if (sent <= 0)
        {
            return false;
        }

        offset +=
            static_cast<std::size_t>(
                sent
            );
    }

    return true;
}

int main()
{
    int fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (fd == -1)
    {
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
        close(fd);

        return 1;
    }

    if (
        connect(
            fd,
            reinterpret_cast<sockaddr*>(
                &address
            ),
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

    std::string nickname;

    std::cout
        << "Nickname: ";

    std::getline(
        std::cin,
        nickname
    );

    if (
        !sendAll(
            fd,
            "NICK "
                + nickname
                + "\n"
        )
    )
    {
        close(fd);

        return 1;
    }

    std::atomic<bool> running{
        true
    };

    std::thread receiver(
        [&]()
        {
            char buffer[4096];

            while (running.load())
            {
                ssize_t received =
                    recv(
                        fd,
                        buffer,
                        sizeof(buffer),
                        0
                    );

                if (received > 0)
                {
                    std::cout.write(
                        buffer,
                        received
                    );

                    std::cout.flush();

                    continue;
                }

                running.store(false);

                break;
            }
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
            !sendAll(
                fd,
                line + "\n"
            )
        )
        {
            break;
        }

        if (line == "/quit")
        {
            break;
        }
    }

    running.store(false);

    shutdown(
        fd,
        SHUT_RDWR
    );

    close(fd);

    if (
        receiver.joinable()
    )
    {
        receiver.join();
    }

    return 0;
}