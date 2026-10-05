#include "chat_protocol.h"

#include <cctype>
#include <utility>

namespace chat
{

std::vector<std::string> extractLines(
    std::string& buffer
)
{
    std::vector<std::string> lines;

    while (true)
    {
        const std::size_t position =
            buffer.find('\n');

        if (position == std::string::npos)
        {
            break;
        }

        std::string line =
            buffer.substr(
                0,
                position
            );

        buffer.erase(
            0,
            position + 1
        );

        if (!line.empty() &&
            line.back() == '\r')
        {
            line.pop_back();
        }

        lines.push_back(
            std::move(line)
        );
    }

    return lines;
}

bool isValidNickname(
    std::string_view nickname
)
{
    if (nickname.empty() ||
        nickname.size() >
            MaxNicknameLength)
    {
        return false;
    }

    for (char ch : nickname)
    {
        const auto unsignedChar =
            static_cast<unsigned char>(ch);

        if (!std::isalnum(unsignedChar) &&
            ch != '_' &&
            ch != '-')
        {
            return false;
        }
    }

    return true;
}

std::string makeChatMessage(
    std::string_view nickname,
    std::string_view message
)
{
    return "["
        + std::string(nickname)
        + "] "
        + std::string(message)
        + "\n";
}

}