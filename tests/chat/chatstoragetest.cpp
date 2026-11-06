#include <gtest/gtest.h>

#include "chat/domain/conversationrepository.h"
#include "chat/domain/messagerepository.h"
#include "chatfixture.h"
#include "dbtoolkit/dbstorage.h"
#include "dbtoolkit/query/pragma.h"

using namespace chat;
using namespace chat_test;

class ChatStorageTest : public ChatFixture
{
protected:
    BackendSession session(const QString& id, int start = 0) const
    {
        return BackendSession { id, start };
    }
};

TEST_F(ChatStorageTest, Open_CreatesTheSchemaAtTheCurrentVersion)
{
    EXPECT_TRUE(m_storage->isOpen());
    EXPECT_EQ(onWorker([this] { return m_storage->schemaVersion(); }), ChatStorage::kSchemaVersion);
}

TEST_F(ChatStorageTest, Open_OnAnEmptyFile_StartsWithoutConversations)
{
    EXPECT_TRUE(awaitOk(m_store->conversations()).empty());
    EXPECT_TRUE(m_storage->droppedSessions().isEmpty());
}

TEST_F(ChatStorageTest, ConnectionName_IsPrefixedWithTheModuleName)
{
    EXPECT_EQ(m_storage->connectionName(), QStringLiteral("chat_chattest"));
}

TEST_F(ChatStorageTest, Open_WhenAlreadyOpen_ReturnsFalse)
{
    EXPECT_FALSE(openStorage());
    EXPECT_TRUE(m_storage->isOpen());
}

TEST_F(ChatStorageTest, Close_MakesTheStorageClosedAndAllowsReopening)
{
    m_store.reset();
    closeStorage();
    EXPECT_FALSE(m_storage->isOpen());

    EXPECT_TRUE(openStorage());
    EXPECT_TRUE(m_storage->isOpen());
}

TEST_F(ChatStorageTest, Reopen_KeepsConversationsAndMessages)
{
    const Conversation conversation = newConversation(QStringLiteral("Kept"));
    appendMessage(conversation.id(), Role::User, QStringLiteral("hello"));

    reopenStorage();

    const std::vector<Conversation> conversations = awaitOk(m_store->conversations());
    ASSERT_EQ(conversations.size(), 1u);
    EXPECT_EQ(conversations.front(), conversation);
    ASSERT_EQ(messagesOf(conversation.id()).size(), 1u);
}

TEST_F(ChatStorageTest, Reopen_AtTheSameVersion_DoesNotRunTheMigrationAgain)
{
    newConversation();

    reopenStorage();
    reopenStorage();

    EXPECT_EQ(awaitOk(m_store->conversations()).size(), 1u);
    EXPECT_EQ(onWorker([this] { return m_storage->schemaVersion(); }), ChatStorage::kSchemaVersion);
}

TEST_F(ChatStorageTest, Open_OnADatabaseFromANewerVersion_IsRefused)
{
    onWorker(
        [this]
        {
            Pragma(QStringLiteral("user_version"))
                .set(ChatStorage::kSchemaVersion + 1)
                .execute(m_storage->storage().database());
            return true;
        });
    m_store.reset();
    closeStorage();

    EXPECT_FALSE(openStorage());
    EXPECT_FALSE(m_storage->isOpen());
}

TEST_F(ChatStorageTest, Open_RunningMessageBecomesInterrupted)
{
    const Conversation conversation = newConversation();
    appendMessage(conversation.id(), Role::User, QStringLiteral("question"));
    appendRunningAnswer(conversation.id());

    reopenStorage();

    const std::vector<Message> messages = messagesOf(conversation.id());
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_EQ(messages[0].status(), MessageStatus::Complete);
    EXPECT_EQ(messages[1].status(), MessageStatus::Interrupted);
}

TEST_F(ChatStorageTest, Open_RunningStepBecomesFailedAndFinishedStepsStay)
{
    const Conversation conversation = newConversation();
    appendMessage(conversation.id(), Role::User, QStringLiteral("question"));
    appendRunningAnswer(conversation.id(),
                        { toolStep(StepStatus::Ok), toolStep(StepStatus::Running) });

    reopenStorage();

    const std::vector<Message> messages = messagesOf(conversation.id());
    ASSERT_EQ(messages.size(), 2u);
    ASSERT_EQ(messages[1].steps().size(), 2u);
    EXPECT_EQ(messages[1].steps()[0].status, StepStatus::Ok);
    EXPECT_EQ(messages[1].steps()[1].status, StepStatus::Failed);
}

TEST_F(ChatStorageTest, Open_CompleteFailedAndCancelledMessagesStayAsTheyWere)
{
    const Conversation conversation = newConversation();
    appendMessage(conversation.id(), Role::User, QStringLiteral("question"));
    appendMessage(conversation.id(), Role::Assistant, QStringLiteral("a"), MessageStatus::Complete);
    appendMessage(conversation.id(), Role::Assistant, QString(), MessageStatus::Failed);
    appendMessage(conversation.id(), Role::Assistant, QString(), MessageStatus::Cancelled);

    reopenStorage();

    const std::vector<Message> messages = messagesOf(conversation.id());
    ASSERT_EQ(messages.size(), 4u);
    EXPECT_EQ(messages[1].status(), MessageStatus::Complete);
    EXPECT_EQ(messages[2].status(), MessageStatus::Failed);
    EXPECT_EQ(messages[3].status(), MessageStatus::Cancelled);
}

TEST_F(ChatStorageTest, Open_ConversationWithARunningMessage_LosesItsSessionAndReportsIt)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), session("s1", 2));
    appendMessage(conversation.id(), Role::User, QStringLiteral("next"));
    appendRunningAnswer(conversation.id());

    reopenStorage();

    EXPECT_EQ(m_storage->droppedSessions(), QStringList { QStringLiteral("s1") });
    EXPECT_FALSE(sessionOf(conversation.id()).has_value());
}

TEST_F(ChatStorageTest, Open_ConversationWithoutARunningMessage_KeepsItsSession)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), session("s1", 2));

    reopenStorage();

    EXPECT_TRUE(m_storage->droppedSessions().isEmpty());
    ASSERT_TRUE(sessionOf(conversation.id()).has_value());
    EXPECT_EQ(*sessionOf(conversation.id()), session("s1", 2));
}

TEST_F(ChatStorageTest, Open_ReportsOnlyTheSessionsOfInterruptedConversations)
{
    const Conversation interrupted = conversationWithSession(QStringLiteral("A"), session("s1"));
    appendRunningAnswer(interrupted.id());
    const Conversation untouched = conversationWithSession(QStringLiteral("B"), session("s2"));
    const Conversation interruptedWithoutSession = newConversation(QStringLiteral("C"));
    appendRunningAnswer(interruptedWithoutSession.id());

    reopenStorage();

    EXPECT_EQ(m_storage->droppedSessions(), QStringList { QStringLiteral("s1") });
    EXPECT_FALSE(sessionOf(interrupted.id()).has_value());
    EXPECT_TRUE(sessionOf(untouched.id()).has_value());
}

TEST_F(ChatStorageTest, Open_AfterTheInterruptionIsDone_ReportsNothingTheSecondTime)
{
    const Conversation conversation = conversationWithSession(QStringLiteral("A"), session("s1"));
    appendRunningAnswer(conversation.id());
    reopenStorage();
    ASSERT_FALSE(m_storage->droppedSessions().isEmpty());

    reopenStorage();

    EXPECT_TRUE(m_storage->droppedSessions().isEmpty());
}
