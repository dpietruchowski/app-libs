#include <gtest/gtest.h>

#include "chat/domain/conversation.h"
#include "chat/domain/message.h"
#include "chatfixture.h"

namespace
{
using chat_test::kNow;

chat::Conversation conversation()
{
    return chat::Conversation::create({ QStringLiteral("T"), {} }, kNow);
}
}

TEST(ConversationTest, Create_StampsBothTimesWithNow)
{
    const chat::Conversation created = conversation();

    EXPECT_EQ(created.createdAt(), kNow);
    EXPECT_EQ(created.updatedAt(), kNow);
}

TEST(ConversationTest, Create_GivesEveryConversationItsOwnId)
{
    const chat::Conversation first = conversation();
    const chat::Conversation second = conversation();

    EXPECT_FALSE(first.id().isNull());
    EXPECT_NE(first.id(), second.id());
}

TEST(ConversationTest, Create_KeepsTitleAndOrigin)
{
    const chat::ChatContext origin { QStringLiteral("course"), QStringLiteral("Physics"), {} };

    const chat::Conversation created
        = chat::Conversation::create({ QStringLiteral("Hello"), origin }, kNow);

    EXPECT_EQ(created.title(), QStringLiteral("Hello"));
    ASSERT_TRUE(created.origin().has_value());
    EXPECT_EQ(*created.origin(), origin);
}

TEST(ConversationTest, Create_StartsWithoutBackendSession)
{
    EXPECT_FALSE(conversation().backendSession().has_value());
}

TEST(ConversationTest, Touch_MovesUpdatedAtAndLeavesCreatedAt)
{
    chat::Conversation touched = conversation();
    const QDateTime later = kNow.addSecs(90);

    touched.touch(later);

    EXPECT_EQ(touched.updatedAt(), later);
    EXPECT_EQ(touched.createdAt(), kNow);
}

TEST(ConversationTest, Touch_LeavesTheDataAndTheSessionAlone)
{
    chat::Conversation touched = conversation();
    touched.setBackendSession(chat::BackendSession { QStringLiteral("s1"), 4 });
    const chat::Conversation::Data before = touched.data();

    touched.touch(kNow.addSecs(5));

    EXPECT_EQ(touched.data(), before);
    ASSERT_TRUE(touched.backendSession().has_value());
    EXPECT_EQ(touched.backendSession()->id, QStringLiteral("s1"));
}

TEST(ConversationTest, Rename_ChangesOnlyTheTitle)
{
    const chat::ChatContext origin { QStringLiteral("course"), QStringLiteral("Physics"), {} };
    chat::Conversation renamed
        = chat::Conversation::create({ QStringLiteral("Old"), origin }, kNow);
    renamed.setBackendSession(chat::BackendSession { QStringLiteral("s1"), 2 });
    const chat::Conversation before = renamed;

    renamed.rename(QStringLiteral("New"));

    EXPECT_EQ(renamed.title(), QStringLiteral("New"));
    EXPECT_EQ(renamed.id(), before.id());
    EXPECT_EQ(renamed.origin(), before.origin());
    EXPECT_EQ(renamed.createdAt(), before.createdAt());
    EXPECT_EQ(renamed.updatedAt(), before.updatedAt());
    EXPECT_EQ(renamed.backendSession(), before.backendSession());
}

TEST(ConversationTest, SetBackendSession_StoresAndClearsIt)
{
    chat::Conversation target = conversation();

    target.setBackendSession(chat::BackendSession { QStringLiteral("s1"), 6 });
    ASSERT_TRUE(target.backendSession().has_value());
    EXPECT_EQ(target.backendSession()->start, 6);

    target.setBackendSession(std::nullopt);
    EXPECT_FALSE(target.backendSession().has_value());
}

TEST(ConversationTest, Equality_ComparesTheBackendSessionToo)
{
    const chat::Conversation plain = conversation();
    chat::Conversation withSession = plain;
    withSession.setBackendSession(chat::BackendSession { QStringLiteral("s1"), 0 });

    EXPECT_NE(plain, withSession);
}

TEST(MessageTest, Create_StampsCreatedAtAndGeneratesAnId)
{
    chat::Message::Data data;
    data.text = QStringLiteral("Hi");

    const chat::Message first = chat::Message::create(data, kNow);
    const chat::Message second = chat::Message::create(data, kNow);

    EXPECT_EQ(first.createdAt(), kNow);
    EXPECT_FALSE(first.id().isNull());
    EXPECT_NE(first.id(), second.id());
}

TEST(MessageTest, Accessors_ReflectTheData)
{
    chat::Message::Data data;
    data.conversationId = QUuid::createUuid();
    data.position = 3;
    data.role = chat::Role::Assistant;
    data.text = QStringLiteral("Answer");
    data.status = chat::MessageStatus::Failed;
    data.error = QStringLiteral("timeout");
    data.steps = { chat_test::toolStep() };

    const chat::Message message = chat::Message::create(data, kNow);

    EXPECT_EQ(message.conversationId(), data.conversationId);
    EXPECT_EQ(message.position(), 3);
    EXPECT_EQ(message.role(), chat::Role::Assistant);
    EXPECT_EQ(message.text(), QStringLiteral("Answer"));
    EXPECT_EQ(message.status(), chat::MessageStatus::Failed);
    EXPECT_EQ(message.error(), QStringLiteral("timeout"));
    EXPECT_EQ(message.steps().size(), 1u);
    EXPECT_EQ(message.data(), data);
}
