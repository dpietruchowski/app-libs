#include <gtest/gtest.h>

#include "chat/domain/conversationtitle.h"

TEST(ConversationTitleTest, ShortMessage_IsKeptAsIs)
{
    EXPECT_EQ(chat::conversationTitle(QStringLiteral("How do I learn faster?")),
              QStringLiteral("How do I learn faster?"));
}

TEST(ConversationTitleTest, Whitespace_IsCollapsed)
{
    EXPECT_EQ(chat::conversationTitle(QStringLiteral("  one\n\ntwo \t three  ")),
              QStringLiteral("one two three"));
}

TEST(ConversationTitleTest, ExactlyAtTheLimit_IsKept)
{
    const QString text(chat::kMaxConversationTitleLength, QLatin1Char('a'));

    EXPECT_EQ(chat::conversationTitle(text), text);
}

TEST(ConversationTitleTest, OneOverTheLimit_IsCutAndEndsWithAnEllipsis)
{
    const QString text(chat::kMaxConversationTitleLength + 1, QLatin1Char('a'));

    const QString title = chat::conversationTitle(text);

    EXPECT_EQ(title.size(), chat::kMaxConversationTitleLength);
    EXPECT_TRUE(title.endsWith(QChar(0x2026)));
    EXPECT_EQ(title.left(chat::kMaxConversationTitleLength - 1),
              QString(chat::kMaxConversationTitleLength - 1, QLatin1Char('a')));
}

TEST(ConversationTitleTest, CutInsideASpace_DoesNotLeaveATrailingSpaceBeforeTheEllipsis)
{
    const QString text = QString(chat::kMaxConversationTitleLength - 2, QLatin1Char('a'))
        + QStringLiteral(" tail of the message");

    const QString title = chat::conversationTitle(text);

    EXPECT_TRUE(title.endsWith(QStringLiteral("a…")));
}

TEST(ConversationTitleTest, BlankMessage_GivesAnEmptyTitle)
{
    EXPECT_TRUE(chat::conversationTitle(QStringLiteral("  \n ")).isEmpty());
}
