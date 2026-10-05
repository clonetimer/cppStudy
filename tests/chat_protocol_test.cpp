#include "chat_protocol.h"

#include <gtest/gtest.h>

#include <string>

TEST(
    ChatProtocolTest,
    ExtractsCompleteLine
)
{
    std::string buffer =
        "hello\n";

    auto lines =
        chat::extractLines(
            buffer
        );

    ASSERT_EQ(
        lines.size(),
        1
    );

    EXPECT_EQ(
        lines[0],
        "hello"
    );

    EXPECT_TRUE(
        buffer.empty()
    );
}

TEST(
    ChatProtocolTest,
    KeepsIncompleteLine
)
{
    std::string buffer =
        "partial";

    auto lines =
        chat::extractLines(
            buffer
        );

    EXPECT_TRUE(
        lines.empty()
    );

    EXPECT_EQ(
        buffer,
        "partial"
    );
}

TEST(
    ChatProtocolTest,
    HandlesTcpSplitAndCombinedMessages
)
{
    std::string buffer =
        "hel";

    auto first =
        chat::extractLines(
            buffer
        );

    EXPECT_TRUE(
        first.empty()
    );

    buffer +=
        "lo\nworld\npar";

    auto second =
        chat::extractLines(
            buffer
        );

    ASSERT_EQ(
        second.size(),
        2
    );

    EXPECT_EQ(
        second[0],
        "hello"
    );

    EXPECT_EQ(
        second[1],
        "world"
    );

    EXPECT_EQ(
        buffer,
        "par"
    );
}

TEST(
    ChatProtocolTest,
    HandlesCrLf
)
{
    std::string buffer =
        "hello\r\n";

    auto lines =
        chat::extractLines(
            buffer
        );

    ASSERT_EQ(
        lines.size(),
        1
    );

    EXPECT_EQ(
        lines[0],
        "hello"
    );
}

TEST(
    ChatProtocolTest,
    AcceptsValidNicknames
)
{
    EXPECT_TRUE(
        chat::isValidNickname(
            "Alice"
        )
    );

    EXPECT_TRUE(
        chat::isValidNickname(
            "user_123"
        )
    );

    EXPECT_TRUE(
        chat::isValidNickname(
            "bob-test"
        )
    );
}

TEST(
    ChatProtocolTest,
    RejectsInvalidNicknames
)
{
    EXPECT_FALSE(
        chat::isValidNickname(
            ""
        )
    );

    EXPECT_FALSE(
        chat::isValidNickname(
            "hello world"
        )
    );

    EXPECT_FALSE(
        chat::isValidNickname(
            "user/name"
        )
    );

    const std::string tooLong(
        chat::MaxNicknameLength + 1,
        'a'
    );

    EXPECT_FALSE(
        chat::isValidNickname(
            tooLong
        )
    );
}

TEST(
    ChatProtocolTest,
    FormatsChatMessage
)
{
    EXPECT_EQ(
        chat::makeChatMessage(
            "Alice",
            "hello"
        ),
        "[Alice] hello\n"
    );
}