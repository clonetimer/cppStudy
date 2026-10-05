#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace chat
{

inline constexpr std::size_t MaxNicknameLength = 32;

inline constexpr std::size_t MaxMessageLength = 4096;

inline constexpr std::size_t MaxReadBufferSize =
    64 * 1024;

inline constexpr std::size_t MaxWriteBufferSize =
    256 * 1024;

std::vector<std::string> extractLines(
    std::string& buffer
);

bool isValidNickname(
    std::string_view nickname
);

std::string makeChatMessage(
    std::string_view nickname,
    std::string_view message
);

}

#endif