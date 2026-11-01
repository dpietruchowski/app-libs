#include <gtest/gtest.h>

#include <QJsonArray>

#include "chat/infrastructure/stepserializer.h"
#include "chatfixture.h"
#include "dbtoolkit/dbstorage.h"
#include "dbtoolkit/query/select.h"

using namespace chat;
using namespace chat_test;

class ChatStoreTest : public ChatFixture
{
protected:
    size_t storedStepCount()
    {
        return onWorker(
            [this]
            {
                Select steps({ StepSerializer::kId });
                steps.from(StepSerializer::kTable);
                return static_cast<size_t>(m_storage->storage().execute(steps).size());
            });
    }

    Step stepOf(const Message& message, size_t index) const { return message.steps().at(index); }
};

TEST_F(ChatStoreTest, CreateConversation_StoresTitleAndStampsBothTimes)
{
    const Conversation created = newConversation(QStringLiteral("Physics"));

    const Conversation found = awaitOk(m_store->conversation(created.id()));
    EXPECT_EQ(found.title(), QStringLiteral("Physics"));
    EXPECT_EQ(found.createdAt(), kNow);
    EXPECT_EQ(found.updatedAt(), kNow);
    EXPECT_FALSE(found.origin().has_value());
    EXPECT_FALSE(found.backendSession().has_value());
}

TEST_F(ChatStoreTest, CreateConversation_KeepsTheOriginWithItsData)
{
    const ChatContext origin { QStringLiteral("course"), QStringLiteral("Physics"),
                               QJsonObject { { QStringLiteral("id"), 7 } } };

    const Conversation created
        = awaitOk(m_store->createConversation(QStringLiteral("Hello"), origin));

    const Conversation found = awaitOk(m_store->conversation(created.id()));
    ASSERT_TRUE(found.origin().has_value());
    EXPECT_EQ(*found.origin(), origin);
}

TEST_F(ChatStoreTest, Conversation_UnknownId_Fails)
{
    EXPECT_TRUE(await(m_store->conversation(QUuid::createUuid())).isFailure());
}

TEST_F(ChatStoreTest, Conversations_AreOrderedByLastUpdateNewestFirst)
{
    const Conversation first = newConversation(QStringLiteral("first"));
    advanceClock(10);
    const Conversation second = newConversation(QStringLiteral("second"));
    advanceClock(10);
    appendMessage(first.id(), Role::User, QStringLiteral("bump"));

    const std::vector<Conversation> conversations = awaitOk(m_store->conversations());

    ASSERT_EQ(conversations.size(), 2u);
    EXPECT_EQ(conversations[0].id(), first.id());
    EXPECT_EQ(conversations[1].id(), second.id());
}

TEST_F(ChatStoreTest, Conversations_CreatedInTheSameSecond_NewestFirst)
{
    const Conversation first = newConversation(QStringLiteral("first"));
    const Conversation second = newConversation(QStringLiteral("second"));

    const std::vector<Conversation> conversations = awaitOk(m_store->conversations());

    ASSERT_EQ(conversations.size(), 2u);
    EXPECT_EQ(conversations[0].id(), second.id());
    EXPECT_EQ(conversations[1].id(), first.id());
}

TEST_F(ChatStoreTest, AppendMessage_NumbersMessagesPerConversation)
{
    const Conversation one = newConversation();
    const Conversation other = newConversation();

    const Message first = appendMessage(one.id(), Role::User, QStringLiteral("a"));
    const Message second = appendMessage(one.id(), Role::Assistant, QStringLiteral("b"));
    const Message elsewhere = appendMessage(other.id(), Role::User, QStringLiteral("c"));

    EXPECT_EQ(first.position(), 0);
    EXPECT_EQ(second.position(), 1);
    EXPECT_EQ(elsewhere.position(), 0);
}

TEST_F(ChatStoreTest, AppendMessage_IgnoresThePositionTheCallerPassed)
{
    const Conversation conversation = newConversation();
    Message::Data data;
    data.conversationId = conversation.id();
    data.position = 42;

    const Message appended = awaitOk(m_store->appendMessage(data));

    EXPECT_EQ(appended.position(), 0);
}

TEST_F(ChatStoreTest, AppendMessage_TouchesTheConversationButNotItsCreationTime)
{
    const Conversation conversation = newConversation();
    advanceClock(30);

    const Message appended = appendMessage(conversation.id(), Role::User, QStringLiteral("a"));

    const Conversation found = awaitOk(m_store->conversation(conversation.id()));
    EXPECT_EQ(found.updatedAt(), kNow.addSecs(30));
    EXPECT_EQ(found.createdAt(), kNow);
    EXPECT_EQ(appended.createdAt(), kNow.addSecs(30));
}

TEST_F(ChatStoreTest, AppendMessage_ToAnUnknownConversation_Fails)
{
    Message::Data data;
    data.conversationId = QUuid::createUuid();

    EXPECT_TRUE(await(m_store->appendMessage(data)).isFailure());
}

TEST_F(ChatStoreTest, AppendMessage_RoundTripsEveryField)
{
    const Conversation conversation = newConversation();
    Message::Data data;
    data.conversationId = conversation.id();
    data.role = Role::Assistant;
    data.text = QStringLiteral("Answer with \"quotes\" and ünïcode");
    data.status = MessageStatus::Failed;
    data.error = QStringLiteral("timeout");
    data.context = ChatContext { QStringLiteral("lesson"), QStringLiteral("Optics"),
                                 QJsonObject { { QStringLiteral("page"), 3 } } };

    const Message appended = awaitOk(m_store->appendMessage(data));

    const std::vector<Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.size(), 1u);
    EXPECT_EQ(stored.front(), appended);
    EXPECT_EQ(stored.front().data().text, data.text);
    EXPECT_EQ(stored.front().error(), QStringLiteral("timeout"));
    ASSERT_TRUE(stored.front().context().has_value());
    EXPECT_EQ(*stored.front().context(), *data.context);
}

TEST_F(ChatStoreTest, Messages_ComeInPositionOrder)
{
    const Conversation conversation = newConversation();
    appendMessage(conversation.id(), Role::User, QStringLiteral("one"));
    appendMessage(conversation.id(), Role::Assistant, QStringLiteral("two"));
    appendMessage(conversation.id(), Role::User, QStringLiteral("three"));

    const std::vector<Message> stored = messagesOf(conversation.id());

    ASSERT_EQ(stored.size(), 3u);
    EXPECT_EQ(stored[0].text(), QStringLiteral("one"));
    EXPECT_EQ(stored[1].text(), QStringLiteral("two"));
    EXPECT_EQ(stored[2].text(), QStringLiteral("three"));
}

TEST_F(ChatStoreTest, Messages_OfAnUnknownConversation_AreEmpty)
{
    EXPECT_TRUE(messagesOf(QUuid::createUuid()).empty());
}

TEST_F(ChatStoreTest, Messages_BelongOnlyToTheirOwnConversation)
{
    const Conversation one = newConversation();
    const Conversation other = newConversation();
    appendMessage(one.id(), Role::User, QStringLiteral("mine"));
    appendMessage(other.id(), Role::User, QStringLiteral("theirs"));

    const std::vector<Message> stored = messagesOf(one.id());

    ASSERT_EQ(stored.size(), 1u);
    EXPECT_EQ(stored.front().text(), QStringLiteral("mine"));
}

TEST_F(ChatStoreTest, AppendMessage_WithSteps_StoresThemInOrder)
{
    const Conversation conversation = newConversation();
    const Step first = toolStep(StepStatus::Ok);
    const Step second = toolStep(StepStatus::Running);

    appendRunningAnswer(conversation.id(), { first, second });

    const std::vector<Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.size(), 1u);
    ASSERT_EQ(stored.front().steps().size(), 2u);
    EXPECT_EQ(stepOf(stored.front(), 0), first);
    EXPECT_EQ(stepOf(stored.front(), 1), second);
}

TEST_F(ChatStoreTest, SaveStep_AppendsNewStepsAfterTheExistingOnes)
{
    const Conversation conversation = newConversation();
    const Message answer = appendRunningAnswer(conversation.id());
    const Step first = toolStep();
    const Step second = toolStep();

    awaitOk(m_store->saveStep(answer.id(), first));
    awaitOk(m_store->saveStep(answer.id(), second));

    const std::vector<Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.front().steps().size(), 2u);
    EXPECT_EQ(stepOf(stored.front(), 0).id, first.id);
    EXPECT_EQ(stepOf(stored.front(), 1).id, second.id);
}

TEST_F(ChatStoreTest, SaveStep_ForAKnownStep_UpdatesItInPlace)
{
    const Conversation conversation = newConversation();
    const Message answer = appendRunningAnswer(conversation.id());
    Step step = toolStep();
    awaitOk(m_store->saveStep(answer.id(), step));
    awaitOk(m_store->saveStep(answer.id(), toolStep()));

    step.status = StepStatus::Ok;
    step.output = QJsonObject { { QStringLiteral("count"), 2 } };
    awaitOk(m_store->saveStep(answer.id(), step));

    const std::vector<Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.front().steps().size(), 2u);
    EXPECT_EQ(stepOf(stored.front(), 0), step);
    EXPECT_EQ(storedStepCount(), 2u);
}

TEST_F(ChatStoreTest, SaveStep_KeepsTheParentOfANestedStep)
{
    const Conversation conversation = newConversation();
    const Message answer = appendRunningAnswer(conversation.id());
    Step parent = toolStep();
    parent.kind = StepKind::Delegation;
    Step child = toolStep();
    child.parentId = parent.id;

    awaitOk(m_store->saveStep(answer.id(), parent));
    awaitOk(m_store->saveStep(answer.id(), child));

    const std::vector<Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.front().steps().size(), 2u);
    EXPECT_FALSE(stepOf(stored.front(), 0).parentId.has_value());
    EXPECT_EQ(stepOf(stored.front(), 1).parentId, parent.id);
}

TEST_F(ChatStoreTest, SaveStep_KeepsWhetherTheToolReadsOrWrites)
{
    const Conversation conversation = newConversation();
    const Message answer = appendRunningAnswer(conversation.id());
    Step writes = toolStep();
    writes.effect = ToolEffect::Writes;
    Step reads = toolStep();

    awaitOk(m_store->saveStep(answer.id(), writes));
    awaitOk(m_store->saveStep(answer.id(), reads));

    const std::vector<Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.front().steps().size(), 2u);
    EXPECT_EQ(stepOf(stored.front(), 0).effect, ToolEffect::Writes);
    EXPECT_EQ(stepOf(stored.front(), 1).effect, ToolEffect::Reads);
}

TEST_F(ChatStoreTest, SaveStep_RoundTripsInputAndEveryKindOfOutput)
{
    const Conversation conversation = newConversation();
    const Message answer = appendRunningAnswer(conversation.id());

    Step object = toolStep(StepStatus::Ok);
    object.input = QJsonObject { { QStringLiteral("q"), QStringLiteral("light") },
                                 { QStringLiteral("tags"), QJsonArray { 1, 2 } } };
    object.output = QJsonObject { { QStringLiteral("hits"), 5 } };
    Step array = toolStep(StepStatus::Ok);
    array.output = QJsonArray { QStringLiteral("a"), QStringLiteral("b") };
    Step text = toolStep(StepStatus::Ok);
    text.output = QStringLiteral("plain text");
    Step nothing = toolStep(StepStatus::Ok);
    nothing.output = QJsonValue(QJsonValue::Null);
    Step pending = toolStep(StepStatus::Running);
    pending.output = QJsonValue(QJsonValue::Undefined);

    for (const Step& step : { object, array, text, nothing, pending })
    {
        awaitOk(m_store->saveStep(answer.id(), step));
    }

    const std::vector<Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.front().steps().size(), 5u);
    EXPECT_EQ(stepOf(stored.front(), 0), object);
    EXPECT_EQ(stepOf(stored.front(), 1), array);
    EXPECT_EQ(stepOf(stored.front(), 2), text);
    EXPECT_EQ(stepOf(stored.front(), 3), nothing);
    EXPECT_TRUE(stepOf(stored.front(), 4).output.isUndefined());
}

TEST_F(ChatStoreTest, SaveStep_ForAnUnknownMessage_Fails)
{
    EXPECT_TRUE(await(m_store->saveStep(QUuid::createUuid(), toolStep())).isFailure());
}

TEST_F(ChatStoreTest, FinishMessage_StoresStatusTextAndError)
{
    const Conversation conversation = newConversation();
    const Message answer = appendRunningAnswer(conversation.id());

    const Message finished = awaitOk(
        m_store->finishMessage(answer.id(), MessageStatus::Failed, QStringLiteral("partial"),
                               QStringLiteral("timeout"), std::nullopt));

    EXPECT_EQ(finished.status(), MessageStatus::Failed);
    const std::vector<Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.size(), 1u);
    EXPECT_EQ(stored.front(), finished);
    EXPECT_EQ(stored.front().text(), QStringLiteral("partial"));
    EXPECT_EQ(stored.front().error(), QStringLiteral("timeout"));
    EXPECT_EQ(stored.front().id(), answer.id());
    EXPECT_EQ(stored.front().position(), answer.position());
}

TEST_F(ChatStoreTest, FinishMessage_KeepsTheStepsRecordedSoFar)
{
    const Conversation conversation = newConversation();
    const Message answer = appendRunningAnswer(conversation.id());
    awaitOk(m_store->saveStep(answer.id(), toolStep(StepStatus::Ok)));

    awaitOk(m_store->finishMessage(answer.id(), MessageStatus::Complete, QStringLiteral("done"),
                                   QString(), std::nullopt));

    EXPECT_EQ(messagesOf(conversation.id()).front().steps().size(), 1u);
}

TEST_F(ChatStoreTest, FinishMessage_StoresTheBackendSession)
{
    const Conversation conversation = newConversation();
    const Message answer = appendRunningAnswer(conversation.id());

    awaitOk(m_store->finishMessage(answer.id(), MessageStatus::Complete, QStringLiteral("done"),
                                   QString(), BackendSession { QStringLiteral("s1"), 4 }));

    ASSERT_TRUE(sessionOf(conversation.id()).has_value());
    EXPECT_EQ(*sessionOf(conversation.id()), (BackendSession { QStringLiteral("s1"), 4 }));
}

TEST_F(ChatStoreTest, FinishMessage_WithoutASession_ClearsTheStoredOne)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), BackendSession { QStringLiteral("s1"), 0 });
    const Message next = appendRunningAnswer(conversation.id());

    awaitOk(m_store->finishMessage(next.id(), MessageStatus::Failed, QString(),
                                   QStringLiteral("backend_failed"), std::nullopt));

    EXPECT_FALSE(sessionOf(conversation.id()).has_value());
}

TEST_F(ChatStoreTest, FinishMessage_TouchesTheConversation)
{
    const Conversation conversation = newConversation();
    const Message answer = appendRunningAnswer(conversation.id());
    advanceClock(15);

    awaitOk(m_store->finishMessage(answer.id(), MessageStatus::Complete, QStringLiteral("done"),
                                   QString(), std::nullopt));

    EXPECT_EQ(awaitOk(m_store->conversation(conversation.id())).updatedAt(), kNow.addSecs(15));
}

TEST_F(ChatStoreTest, FinishMessage_ForAnUnknownMessage_Fails)
{
    EXPECT_TRUE(await(m_store->finishMessage(QUuid::createUuid(), MessageStatus::Complete,
                                             QStringLiteral("x"), QString(), std::nullopt))
                    .isFailure());
}

TEST_F(ChatStoreTest, RestartMessage_ClearsTheOutcomeAndTheStepsAndRunsAgain)
{
    const Conversation conversation = newConversation();
    const Message answer = appendRunningAnswer(conversation.id(), { toolStep(StepStatus::Ok) });
    awaitOk(m_store->finishMessage(answer.id(), MessageStatus::Failed, QStringLiteral("partial"),
                                   QStringLiteral("timeout"), std::nullopt));

    const Message restarted = awaitOk(m_store->restartMessage(answer.id()));

    EXPECT_EQ(restarted.id(), answer.id());
    EXPECT_EQ(restarted.position(), answer.position());
    EXPECT_EQ(restarted.status(), MessageStatus::Running);
    EXPECT_TRUE(restarted.text().isEmpty());
    EXPECT_TRUE(restarted.error().isEmpty());
    EXPECT_TRUE(restarted.steps().empty());

    const std::vector<Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.size(), 1u);
    EXPECT_EQ(stored.front(), restarted);
    EXPECT_EQ(storedStepCount(), 0u);
}

TEST_F(ChatStoreTest, RestartMessage_ForAnUnknownMessage_Fails)
{
    EXPECT_TRUE(await(m_store->restartMessage(QUuid::createUuid())).isFailure());
}

TEST_F(ChatStoreTest, Rename_StoresTheTrimmedTitleAndKeepsTheLastUpdate)
{
    const Conversation conversation = newConversation(QStringLiteral("Old"));
    advanceClock(60);

    awaitOk(m_store->rename(conversation.id(), QStringLiteral("  New title \n")));

    const Conversation found = awaitOk(m_store->conversation(conversation.id()));
    EXPECT_EQ(found.title(), QStringLiteral("New title"));
    EXPECT_EQ(found.updatedAt(), kNow);
    EXPECT_EQ(found.createdAt(), kNow);
}

TEST_F(ChatStoreTest, Rename_KeepsTheOriginAndTheBackendSession)
{
    const ChatContext origin { QStringLiteral("course"), QStringLiteral("Physics"), {} };
    const Conversation conversation
        = awaitOk(m_store->createConversation(QStringLiteral("Old"), origin));
    const Message answer = appendRunningAnswer(conversation.id());
    awaitOk(m_store->finishMessage(answer.id(), MessageStatus::Complete, QStringLiteral("done"),
                                   QString(), BackendSession { QStringLiteral("s1"), 2 }));

    awaitOk(m_store->rename(conversation.id(), QStringLiteral("New")));

    const Conversation found = awaitOk(m_store->conversation(conversation.id()));
    ASSERT_TRUE(found.origin().has_value());
    EXPECT_EQ(*found.origin(), origin);
    ASSERT_TRUE(found.backendSession().has_value());
    EXPECT_EQ(*found.backendSession(), (BackendSession { QStringLiteral("s1"), 2 }));
}

TEST_F(ChatStoreTest, Rename_ToABlankTitle_IsRejectedAndChangesNothing)
{
    const Conversation conversation = newConversation(QStringLiteral("Old"));

    EXPECT_TRUE(await(m_store->rename(conversation.id(), QStringLiteral("  \t"))).isFailure());

    EXPECT_EQ(awaitOk(m_store->conversation(conversation.id())).title(), QStringLiteral("Old"));
}

TEST_F(ChatStoreTest, Rename_AnUnknownConversation_Fails)
{
    EXPECT_TRUE(await(m_store->rename(QUuid::createUuid(), QStringLiteral("x"))).isFailure());
}

TEST_F(ChatStoreTest, Remove_DeletesTheConversationItsMessagesAndItsSteps)
{
    const Conversation conversation = newConversation();
    appendMessage(conversation.id(), Role::User, QStringLiteral("question"));
    appendRunningAnswer(conversation.id(), { toolStep(StepStatus::Ok) });
    ASSERT_EQ(storedStepCount(), 1u);

    awaitOk(m_store->remove(conversation.id()));

    EXPECT_TRUE(await(m_store->conversation(conversation.id())).isFailure());
    EXPECT_TRUE(messagesOf(conversation.id()).empty());
    EXPECT_EQ(storedStepCount(), 0u);
}

TEST_F(ChatStoreTest, Remove_LeavesOtherConversationsAlone)
{
    const Conversation removed = newConversation(QStringLiteral("gone"));
    const Conversation kept = newConversation(QStringLiteral("kept"));
    appendMessage(kept.id(), Role::User, QStringLiteral("question"));

    awaitOk(m_store->remove(removed.id()));

    const std::vector<Conversation> conversations = awaitOk(m_store->conversations());
    ASSERT_EQ(conversations.size(), 1u);
    EXPECT_EQ(conversations.front().id(), kept.id());
    EXPECT_EQ(messagesOf(kept.id()).size(), 1u);
}

TEST_F(ChatStoreTest, Remove_ReturnsTheBackendSessionThatLeftTheDatabase)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), BackendSession { QStringLiteral("s1"), 3 });

    const std::optional<BackendSession> dropped = awaitOk(m_store->remove(conversation.id()));

    ASSERT_TRUE(dropped.has_value());
    EXPECT_EQ(*dropped, (BackendSession { QStringLiteral("s1"), 3 }));
}

TEST_F(ChatStoreTest, Remove_AConversationWithoutASession_ReturnsNothing)
{
    const Conversation conversation = newConversation();

    EXPECT_FALSE(awaitOk(m_store->remove(conversation.id())).has_value());
}

TEST_F(ChatStoreTest, Remove_AnUnknownConversation_SucceedsWithNothing)
{
    const Result<std::optional<BackendSession>> result
        = await(m_store->remove(QUuid::createUuid()));

    ASSERT_TRUE(result.isSuccess());
    EXPECT_FALSE(result.value().has_value());
}
