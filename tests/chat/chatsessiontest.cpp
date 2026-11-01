#include <gtest/gtest.h>

#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>
#include <memory>
#include <vector>

#include "agent/claudecli.h"
#include "chat/application/agentcatalog.h"
#include "chat/application/agentturn.h"
#include "chat/application/turnrunner.h"
#include "chat/domain/conversationtitle.h"
#include "chat/ui/chat.h"
#include "chat/ui/chatsession.h"
#include "chatfixture.h"
#include "fakeagentbackend.h"

using chat::AppAgent;
using chat::Chat;
using chat::ChatContext;
using chat::ChatMessageModel;
using chat::ChatSession;
using chat::Role;
using chat::TurnRunner;
using namespace chat_test;

namespace
{

constexpr int kWaitMilliseconds = 5000;

const QString kLoginCommand = QStringLiteral("env CLAUDE_CONFIG_DIR=/data/chat claude auth login");

QString idText(const chat::Conversation& conversation)
{
    return conversation.id().toString(QUuid::WithoutBraces);
}

chat::Step delegationStep(chat::StepStatus status)
{
    chat::Step step;
    step.id = QUuid::createUuid();
    step.kind = chat::StepKind::Delegation;
    step.agent = QStringLiteral("courses");
    step.tool = QStringLiteral("ask_courses");
    step.status = status;
    return step;
}

QVariant valueAt(ChatSession& session, int row, ChatMessageModel::Roles role)
{
    ChatMessageModel* model = session.messages();
    return model->data(model->index(row), role);
}

QString textAt(ChatSession& session, int row)
{
    return valueAt(session, row, ChatMessageModel::TextRole).toString();
}

QString statusAt(ChatSession& session, int row)
{
    return valueAt(session, row, ChatMessageModel::StatusRole).toString();
}

QVariantList stepsAt(ChatSession& session, int row)
{
    return valueAt(session, row, ChatMessageModel::StepsRole).toList();
}

int rowsOf(ChatSession& session) { return session.messages()->rowCount(); }

}

class ChatSessionTest : public ChatFixture
{
protected:
    void SetUp() override
    {
        ChatFixture::SetUp();
        m_catalog.add(AppAgent { QStringLiteral("courses"), QStringLiteral("Courses"),
                                 QStringLiteral("Answers questions about courses."),
                                 ::Agent(QStringLiteral("model"), QStringLiteral("prompt")) });
        m_runner = std::make_unique<TurnRunner>(*m_store, m_backend, m_catalog);
        m_settings = std::make_unique<QSettings>(m_directory.filePath(QStringLiteral("chat.ini")),
                                                 QSettings::IniFormat);
        m_chat
            = std::make_unique<Chat>(*m_store, *m_runner, m_catalog, *m_settings,
                                     Chat::Config { QStringLiteral("chat"), kLoginCommand, true });
        m_session = newSession();
        m_worker.drain();
    }

    void TearDown() override
    {
        m_worker.drain();
        m_sessions.clear();
        m_session.reset();
        m_chat.reset();
        m_settings.reset();
        m_runner.reset();
        ChatFixture::TearDown();
    }

    std::unique_ptr<ChatSession> newSession(const QString& conversationId = QString())
    {
        auto session = std::make_unique<ChatSession>();
        session->setChat(m_chat.get());
        session->setConversationId(conversationId);
        return session;
    }

    ChatSession& addSession(const QString& conversationId)
    {
        m_sessions.push_back(newSession(conversationId));
        return *m_sessions.back();
    }

    template <typename Predicate> bool waitUntil(Predicate predicate)
    {
        return QTest::qWaitFor(predicate, kWaitMilliseconds);
    }

    bool waitForTurns(size_t turns)
    {
        return waitUntil(
            [&] { return m_backend.requests.size() >= turns && !m_backend.latest.isNull(); });
    }

    bool waitForIdle(ChatSession& session)
    {
        return waitUntil([&] { return !session.busy(); });
    }

    chat::Conversation conversationWithQuestionAndAnswer(const QString& title)
    {
        const chat::Conversation conversation = newConversation(title);
        appendMessage(conversation.id(), Role::User, QStringLiteral("question"));
        appendMessage(conversation.id(), Role::Assistant, QStringLiteral("answer"));
        return conversation;
    }

    chat::AgentCatalog m_catalog;
    FakeBackend m_backend;
    std::unique_ptr<TurnRunner> m_runner;
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<Chat> m_chat;
    std::unique_ptr<ChatSession> m_session;
    std::vector<std::unique_ptr<ChatSession>> m_sessions;
};

TEST_F(ChatSessionTest, ANewSession_IsIdleAndEmpty)
{
    EXPECT_EQ(m_session->chat(), m_chat.get());
    EXPECT_TRUE(m_session->conversationId().isEmpty());
    EXPECT_TRUE(m_session->title().isEmpty());
    EXPECT_FALSE(m_session->busy());
    EXPECT_TRUE(m_session->activity().isEmpty());
    EXPECT_TRUE(m_session->context().isEmpty());
    EXPECT_EQ(rowsOf(*m_session), 0);
}

TEST_F(ChatSessionTest, SetChat_NotifiesOnlyWhenTheChatChanges)
{
    ChatSession session;
    QSignalSpy changed(&session, &ChatSession::chatChanged);

    session.setChat(m_chat.get());
    session.setChat(m_chat.get());

    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(session.chat(), m_chat.get());
}

TEST_F(ChatSessionTest, SetConversationId_LoadsTheTitleAndTheMessages)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));

    m_session->setConversationId(idText(conversation));

    ASSERT_TRUE(waitUntil(
        [&] { return rowsOf(*m_session) == 2 && m_session->title() == QStringLiteral("Topic"); }));
    EXPECT_EQ(m_session->conversationId(), idText(conversation));
    EXPECT_EQ(textAt(*m_session, 0), QStringLiteral("question"));
    EXPECT_EQ(textAt(*m_session, 1), QStringLiteral("answer"));
    EXPECT_FALSE(m_session->busy());
}

TEST_F(ChatSessionTest, TheIdSetBeforeTheChat_IsLoadedWhenTheChatArrives)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    ChatSession session;
    session.setConversationId(idText(conversation));
    ASSERT_EQ(rowsOf(session), 0);

    session.setChat(m_chat.get());

    ASSERT_TRUE(waitUntil(
        [&] { return rowsOf(session) == 2 && session.title() == QStringLiteral("Topic"); }));
}

TEST_F(ChatSessionTest, SetConversationId_ToTheSameId_ChangesNothing)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    m_session->setConversationId(idText(conversation));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));
    QSignalSpy changed(m_session.get(), &ChatSession::conversationIdChanged);
    QSignalSpy reset(m_session->messages(), &QAbstractItemModel::modelReset);

    m_session->setConversationId(idText(conversation));

    EXPECT_EQ(changed.count(), 0);
    EXPECT_EQ(reset.count(), 0);
    EXPECT_EQ(rowsOf(*m_session), 2);
}

TEST_F(ChatSessionTest, SetConversationId_ToAnotherConversation_ReplacesTheMessages)
{
    const chat::Conversation first = conversationWithQuestionAndAnswer(QStringLiteral("First"));
    const chat::Conversation second = newConversation(QStringLiteral("Second"));
    appendMessage(second.id(), Role::User, QStringLiteral("only question"));
    m_session->setConversationId(idText(first));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_session->setConversationId(idText(second));

    ASSERT_TRUE(waitUntil(
        [&] { return rowsOf(*m_session) == 1 && m_session->title() == QStringLiteral("Second"); }));
    EXPECT_EQ(textAt(*m_session, 0), QStringLiteral("only question"));
}

TEST_F(ChatSessionTest, SetConversationId_ToAnEmptyId_ResetsTheSessionToANewOne)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    m_session->setConversationId(idText(conversation));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_session->setConversationId(QString());

    EXPECT_TRUE(m_session->conversationId().isEmpty());
    EXPECT_TRUE(m_session->title().isEmpty());
    EXPECT_EQ(rowsOf(*m_session), 0);
}

TEST_F(ChatSessionTest, SettingTwoConversationsInARow_ShowsOnlyTheLastOne)
{
    const chat::Conversation first = conversationWithQuestionAndAnswer(QStringLiteral("First"));
    const chat::Conversation second = newConversation(QStringLiteral("Second"));
    appendMessage(second.id(), Role::User, QStringLiteral("one"));
    appendMessage(second.id(), Role::Assistant, QStringLiteral("two"));
    appendMessage(second.id(), Role::User, QStringLiteral("three"));

    m_session->setConversationId(idText(first));
    m_session->setConversationId(idText(second));

    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 3; }));
    QTest::qWait(100);
    EXPECT_EQ(rowsOf(*m_session), 3);
    EXPECT_EQ(m_session->title(), QStringLiteral("Second"));
}

TEST_F(ChatSessionTest, Send_WithoutAChat_DoesNothing)
{
    ChatSession session;

    session.send(QStringLiteral("hello"));

    EXPECT_FALSE(session.busy());
    EXPECT_TRUE(awaitOk(m_store->conversations()).empty());
    EXPECT_TRUE(m_backend.requests.empty());
}

TEST_F(ChatSessionTest, Send_InANewSession_CreatesTheConversationAndRunsTheTurn)
{
    const QString question = QStringLiteral("How do I learn faster?");

    m_session->send(question);

    EXPECT_TRUE(m_session->busy());
    ASSERT_TRUE(waitUntil([&] { return !m_session->conversationId().isEmpty(); }));
    const std::vector<chat::Conversation> conversations = awaitOk(m_store->conversations());
    ASSERT_EQ(conversations.size(), 1u);
    EXPECT_EQ(idText(conversations.front()), m_session->conversationId());
    EXPECT_EQ(conversations.front().title(), chat::conversationTitle(question));
    EXPECT_EQ(m_session->title(), chat::conversationTitle(question));

    ASSERT_TRUE(waitForTurns(1));
    m_backend.latest->complete(QStringLiteral("Practise a little every day."));
    ASSERT_TRUE(waitForIdle(*m_session));
    ASSERT_TRUE(waitUntil(
        [&] {
            return rowsOf(*m_session) == 2 && statusAt(*m_session, 1) == QStringLiteral("complete");
        }));
    EXPECT_EQ(textAt(*m_session, 0), question);
    EXPECT_EQ(textAt(*m_session, 1), QStringLiteral("Practise a little every day."));
}

TEST_F(ChatSessionTest, Send_WhileATurnRuns_IsIgnored)
{
    m_session->send(QStringLiteral("first"));
    ASSERT_TRUE(waitForTurns(1));

    m_session->send(QStringLiteral("second"));

    m_backend.latest->complete(QStringLiteral("answer"));
    ASSERT_TRUE(waitForIdle(*m_session));
    EXPECT_EQ(m_backend.requests.size(), 1u);
    EXPECT_EQ(awaitOk(m_store->conversations()).size(), 1u);
}

TEST_F(ChatSessionTest, Send_WithABlankText_IsIgnored)
{
    m_session->send(QStringLiteral("   "));
    m_session->send(QString());

    EXPECT_FALSE(m_session->busy());
    EXPECT_TRUE(awaitOk(m_store->conversations()).empty());
}

TEST_F(ChatSessionTest, Send_InAnExistingConversation_AppendsToIt)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    m_session->setConversationId(idText(conversation));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_session->send(QStringLiteral("and then?"));

    EXPECT_TRUE(m_session->busy());
    ASSERT_TRUE(waitForTurns(1));
    EXPECT_EQ(m_backend.requests.front().conversationId, conversation.id());
    m_backend.latest->complete(QStringLiteral("then this"));
    ASSERT_TRUE(waitForIdle(*m_session));
    ASSERT_TRUE(waitUntil(
        [&] {
            return rowsOf(*m_session) == 4 && statusAt(*m_session, 3) == QStringLiteral("complete");
        }));
    EXPECT_EQ(textAt(*m_session, 2), QStringLiteral("and then?"));
    EXPECT_EQ(textAt(*m_session, 3), QStringLiteral("then this"));
    EXPECT_EQ(awaitOk(m_store->conversations()).size(), 1u);
}

TEST_F(ChatSessionTest, PartialText_UpdatesTheRunningAnswer)
{
    m_session->send(QStringLiteral("question"));
    ASSERT_TRUE(waitForTurns(1));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_backend.latest->reportPartialText(QStringLiteral("Prac"));
    EXPECT_EQ(textAt(*m_session, 1), QStringLiteral("Prac"));
    m_backend.latest->reportPartialText(QStringLiteral("Practise"));

    EXPECT_EQ(textAt(*m_session, 1), QStringLiteral("Practise"));
    EXPECT_EQ(statusAt(*m_session, 1), QStringLiteral("running"));
    m_backend.latest->complete(QStringLiteral("Practise daily."));
    ASSERT_TRUE(waitForIdle(*m_session));
}

TEST_F(ChatSessionTest, APartialTextThatArrivesBeforeTheMessagesLoad_IsKept)
{
    const chat::Conversation conversation = newConversation(QStringLiteral("Topic"));
    appendMessage(conversation.id(), Role::User, QStringLiteral("question"));
    appendRunningAnswer(conversation.id());

    m_session->setConversationId(idText(conversation));
    emit m_runner->partialText(conversation.id(), QStringLiteral("live text"));

    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));
    EXPECT_EQ(textAt(*m_session, 1), QStringLiteral("live text"));
}

TEST_F(ChatSessionTest, AStepThatArrivesBeforeTheMessagesLoad_IsKept)
{
    const chat::Conversation conversation = newConversation(QStringLiteral("Topic"));
    appendMessage(conversation.id(), Role::User, QStringLiteral("question"));
    appendRunningAnswer(conversation.id());

    m_session->setConversationId(idText(conversation));
    emit m_runner->stepStarted(conversation.id(), delegationStep(chat::StepStatus::Running));

    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));
    EXPECT_EQ(stepsAt(*m_session, 1).size(), 1);
    EXPECT_EQ(m_session->activity(), QStringLiteral("Asking Courses…"));
}

TEST_F(ChatSessionTest, Steps_ShowTheActivityWhileTheyRunAndClearItWhenTheyFinish)
{
    m_session->send(QStringLiteral("question"));
    ASSERT_TRUE(waitForTurns(1));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));
    QSignalSpy activityChanged(m_session.get(), &ChatSession::activityChanged);
    chat::Step step = delegationStep(chat::StepStatus::Running);

    m_backend.latest->reportStepStarted(step);

    EXPECT_EQ(m_session->activity(), QStringLiteral("Asking Courses…"));
    ASSERT_EQ(stepsAt(*m_session, 1).size(), 1);
    EXPECT_EQ(stepsAt(*m_session, 1).at(0).toMap().value(QStringLiteral("agentTitle")).toString(),
              QStringLiteral("Courses"));
    step.status = chat::StepStatus::Ok;
    m_backend.latest->reportStepFinished(step);

    EXPECT_TRUE(m_session->activity().isEmpty());
    ASSERT_EQ(stepsAt(*m_session, 1).size(), 1);
    EXPECT_EQ(stepsAt(*m_session, 1).at(0).toMap().value(QStringLiteral("status")).toString(),
              QStringLiteral("ok"));
    EXPECT_EQ(activityChanged.count(), 2);
    m_backend.latest->complete(QStringLiteral("done"));
    ASSERT_TRUE(waitForIdle(*m_session));
}

TEST_F(ChatSessionTest, TheActivity_IsClearedWhenTheTurnFinishesWhileAStepRuns)
{
    m_session->send(QStringLiteral("question"));
    ASSERT_TRUE(waitForTurns(1));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));
    m_backend.latest->reportStepStarted(delegationStep(chat::StepStatus::Running));
    ASSERT_FALSE(m_session->activity().isEmpty());

    m_backend.latest->complete(QStringLiteral("done"));

    ASSERT_TRUE(waitForIdle(*m_session));
    EXPECT_TRUE(m_session->activity().isEmpty());
}

TEST_F(ChatSessionTest, TheStepsOfAFinishedTurn_AreKeptInItsMessage)
{
    m_session->send(QStringLiteral("question"));
    ASSERT_TRUE(waitForTurns(1));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));
    chat::Step step = delegationStep(chat::StepStatus::Running);
    m_backend.latest->reportStepStarted(step);
    step.status = chat::StepStatus::Ok;
    m_backend.latest->reportStepFinished(step);

    m_backend.latest->complete(QStringLiteral("done"));

    ASSERT_TRUE(waitForIdle(*m_session));
    ASSERT_TRUE(waitUntil([&] { return statusAt(*m_session, 1) == QStringLiteral("complete"); }));
    EXPECT_EQ(stepsAt(*m_session, 1).size(), 1);
}

TEST_F(ChatSessionTest, Cancel_AsksTheTurnToStopAndEndsCancelled)
{
    m_session->send(QStringLiteral("question"));
    ASSERT_TRUE(waitForTurns(1));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_session->cancel();

    EXPECT_EQ(m_backend.latest->cancelCalls, 1);
    m_backend.latest->fail(chat::turn_error::kCancelled);
    ASSERT_TRUE(waitForIdle(*m_session));
    ASSERT_TRUE(waitUntil([&] { return statusAt(*m_session, 1) == QStringLiteral("cancelled"); }));
    EXPECT_EQ(valueAt(*m_session, 1, ChatMessageModel::ErrorRole).toString(),
              QStringLiteral("Cancelled."));
}

TEST_F(ChatSessionTest, Cancel_WithoutAConversation_DoesNothing)
{
    m_session->cancel();

    EXPECT_FALSE(m_session->busy());
}

TEST_F(ChatSessionTest, Retry_RunsAFailedTurnAgain)
{
    m_session->send(QStringLiteral("question"));
    ASSERT_TRUE(waitForTurns(1));
    m_backend.latest->fail(claudecli::kServerError);
    ASSERT_TRUE(waitForIdle(*m_session));
    ASSERT_TRUE(waitUntil(
        [&] {
            return rowsOf(*m_session) == 2 && statusAt(*m_session, 1) == QStringLiteral("failed");
        }));
    EXPECT_EQ(valueAt(*m_session, 1, ChatMessageModel::ErrorRole).toString(),
              QStringLiteral("The provider failed to answer."));

    m_session->retry();

    EXPECT_TRUE(m_session->busy());
    ASSERT_TRUE(waitUntil([&] { return m_backend.requests.size() == 2u; }));
    m_backend.latest->complete(QStringLiteral("now it works"));
    ASSERT_TRUE(waitForIdle(*m_session));
    ASSERT_TRUE(waitUntil([&] { return statusAt(*m_session, 1) == QStringLiteral("complete"); }));
    EXPECT_EQ(rowsOf(*m_session), 2);
    EXPECT_EQ(textAt(*m_session, 1), QStringLiteral("now it works"));
}

TEST_F(ChatSessionTest, Retry_WhenTheLastAnswerIsComplete_DoesNothing)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    m_session->setConversationId(idText(conversation));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_session->retry();
    QTest::qWait(100);

    EXPECT_FALSE(m_session->busy());
    EXPECT_TRUE(m_backend.requests.empty());
    EXPECT_EQ(rowsOf(*m_session), 2);
}

TEST_F(ChatSessionTest, Retry_WithoutAConversation_DoesNothing)
{
    m_session->retry();

    EXPECT_FALSE(m_session->busy());
}

TEST_F(ChatSessionTest, ANotLoggedInFailure_ShowsTheLoginCommandOfTheChat)
{
    m_session->send(QStringLiteral("question"));
    ASSERT_TRUE(waitForTurns(1));

    m_backend.latest->fail(claudecli::kNotLoggedIn);

    ASSERT_TRUE(waitForIdle(*m_session));
    ASSERT_TRUE(waitUntil(
        [&] {
            return rowsOf(*m_session) == 2 && statusAt(*m_session, 1) == QStringLiteral("failed");
        }));
    EXPECT_TRUE(
        valueAt(*m_session, 1, ChatMessageModel::ErrorRole).toString().contains(kLoginCommand));
}

TEST_F(ChatSessionTest, TwoSessionsOnOneConversation_BothFollowTheTurn)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    m_session->setConversationId(idText(conversation));
    ChatSession& other = addSession(idText(conversation));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2 && rowsOf(other) == 2; }));

    m_session->send(QStringLiteral("follow-up"));

    ASSERT_TRUE(waitForTurns(1));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(other) == 4; }));
    EXPECT_TRUE(other.busy());
    m_backend.latest->reportPartialText(QStringLiteral("part"));
    EXPECT_EQ(textAt(other, 3), QStringLiteral("part"));
    EXPECT_EQ(textAt(*m_session, 3), QStringLiteral("part"));
    m_backend.latest->complete(QStringLiteral("whole"));
    ASSERT_TRUE(waitForIdle(*m_session));
    ASSERT_TRUE(waitForIdle(other));
    ASSERT_TRUE(waitUntil(
        [&]
        {
            return textAt(other, 3) == QStringLiteral("whole")
                && textAt(*m_session, 3) == QStringLiteral("whole");
        }));
}

TEST_F(ChatSessionTest, ASessionOfAnotherConversation_IgnoresTheTurn)
{
    const chat::Conversation running = conversationWithQuestionAndAnswer(QStringLiteral("Running"));
    const chat::Conversation idle = conversationWithQuestionAndAnswer(QStringLiteral("Idle"));
    m_session->setConversationId(idText(running));
    ChatSession& other = addSession(idText(idle));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2 && rowsOf(other) == 2; }));

    m_session->send(QStringLiteral("follow-up"));
    ASSERT_TRUE(waitForTurns(1));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 4; }));
    m_backend.latest->reportPartialText(QStringLiteral("part"));
    m_backend.latest->reportStepStarted(delegationStep(chat::StepStatus::Running));

    EXPECT_FALSE(other.busy());
    EXPECT_TRUE(other.activity().isEmpty());
    EXPECT_EQ(rowsOf(other), 2);
    m_backend.latest->complete(QStringLiteral("whole"));
    ASSERT_TRUE(waitForIdle(*m_session));
    QTest::qWait(100);
    EXPECT_FALSE(other.busy());
    EXPECT_EQ(rowsOf(other), 2);
}

TEST_F(ChatSessionTest, ASessionAttachedMidTurn_IsBusyUntilTheTurnEnds)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    ASSERT_TRUE(m_runner->send(conversation.id(), QStringLiteral("follow-up")));
    ASSERT_TRUE(waitForTurns(1));

    ChatSession& late = addSession(idText(conversation));

    EXPECT_TRUE(late.busy());
    ASSERT_TRUE(waitUntil([&] { return rowsOf(late) == 4; }));
    EXPECT_TRUE(late.busy());
    m_backend.latest->reportPartialText(QStringLiteral("part"));
    EXPECT_EQ(textAt(late, 3), QStringLiteral("part"));
    m_backend.latest->complete(QStringLiteral("whole"));
    ASSERT_TRUE(waitForIdle(late));
}

TEST_F(ChatSessionTest, RemovingTheConversation_ResetsTheSessionToANewOne)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    m_session->setConversationId(idText(conversation));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_chat->remove(idText(conversation));

    ASSERT_TRUE(waitUntil([&] { return m_session->conversationId().isEmpty(); }));
    EXPECT_TRUE(m_session->title().isEmpty());
    EXPECT_EQ(rowsOf(*m_session), 0);
    EXPECT_FALSE(m_session->busy());
    EXPECT_TRUE(awaitOk(m_store->conversations()).empty());
}

TEST_F(ChatSessionTest, RemovingAnotherConversation_LeavesTheSessionAlone)
{
    const chat::Conversation shown = conversationWithQuestionAndAnswer(QStringLiteral("Shown"));
    const chat::Conversation other = conversationWithQuestionAndAnswer(QStringLiteral("Other"));
    m_session->setConversationId(idText(shown));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_chat->remove(idText(other));

    ASSERT_TRUE(waitUntil([&] { return awaitOk(m_store->conversations()).size() == 1u; }));
    EXPECT_EQ(m_session->conversationId(), idText(shown));
    EXPECT_EQ(rowsOf(*m_session), 2);
}

TEST_F(ChatSessionTest, TheContext_RoundTripsThroughTheProperty)
{
    QSignalSpy changed(m_session.get(), &ChatSession::contextChanged);
    const QVariantMap context { { QStringLiteral("key"), QStringLiteral("lesson") },
                                { QStringLiteral("title"), QStringLiteral("Lesson 3") },
                                { QStringLiteral("data"),
                                  QVariantMap {
                                      { QStringLiteral("lessonId"), QStringLiteral("l3") } } } };

    m_session->setContext(context);
    m_session->setContext(context);

    EXPECT_EQ(m_session->context(), context);
    EXPECT_EQ(changed.count(), 1);
}

TEST_F(ChatSessionTest, AContextWithoutAKey_ClearsTheContext)
{
    m_session->setContext({ { QStringLiteral("key"), QStringLiteral("lesson") } });
    QSignalSpy changed(m_session.get(), &ChatSession::contextChanged);

    m_session->setContext({ { QStringLiteral("title"), QStringLiteral("Lesson 3") } });

    EXPECT_TRUE(m_session->context().isEmpty());
    EXPECT_EQ(changed.count(), 1);
}

TEST_F(ChatSessionTest, TheContext_IsStoredAsTheOriginAndSentWithTheTurn)
{
    m_session->setContext(
        { { QStringLiteral("key"), QStringLiteral("lesson") },
          { QStringLiteral("title"), QStringLiteral("Lesson 3") },
          { QStringLiteral("data"),
            QVariantMap { { QStringLiteral("lessonId"), QStringLiteral("l3") } } } });
    const ChatContext expected { QStringLiteral("lesson"), QStringLiteral("Lesson 3"),
                                 QJsonObject {
                                     { QStringLiteral("lessonId"), QStringLiteral("l3") } } };

    m_session->send(QStringLiteral("explain this lesson"));

    ASSERT_TRUE(waitForTurns(1));
    EXPECT_EQ(m_backend.requests.front().context, expected);
    const std::vector<chat::Conversation> conversations = awaitOk(m_store->conversations());
    ASSERT_EQ(conversations.size(), 1u);
    EXPECT_EQ(conversations.front().origin(), expected);
    m_backend.latest->complete(QStringLiteral("answer"));
    ASSERT_TRUE(waitForIdle(*m_session));
}

TEST_F(ChatSessionTest, WithoutAContext_NothingIsSentAsOne)
{
    m_session->send(QStringLiteral("question"));

    ASSERT_TRUE(waitForTurns(1));
    EXPECT_FALSE(m_backend.requests.front().context.has_value());
    EXPECT_FALSE(awaitOk(m_store->conversations()).front().origin().has_value());
    m_backend.latest->complete(QStringLiteral("answer"));
    ASSERT_TRUE(waitForIdle(*m_session));
}

TEST_F(ChatSessionTest, SetChat_ToNull_DetachesTheSessionFromTheTurns)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    m_session->setConversationId(idText(conversation));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_session->setChat(nullptr);
    ASSERT_TRUE(m_runner->send(conversation.id(), QStringLiteral("follow-up")));
    ASSERT_TRUE(waitForTurns(1));
    m_backend.latest->reportPartialText(QStringLiteral("part"));
    m_backend.latest->complete(QStringLiteral("whole"));
    ASSERT_TRUE(waitUntil([&] { return !m_runner->isBusy(conversation.id()); }));

    EXPECT_FALSE(m_session->busy());
    EXPECT_EQ(rowsOf(*m_session), 2);
    m_session->send(QStringLiteral("ignored"));
    EXPECT_EQ(m_backend.requests.size(), 1u);
}

TEST_F(ChatSessionTest, ASessionOutlivingTheChat_IgnoresSendCancelAndRetry)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    m_session->setConversationId(idText(conversation));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_chat.reset();

    EXPECT_EQ(m_session->chat(), nullptr);
    m_session->send(QStringLiteral("hello"));
    m_session->cancel();
    m_session->retry();
    EXPECT_FALSE(m_session->busy());
    EXPECT_EQ(rowsOf(*m_session), 2);
    EXPECT_EQ(textAt(*m_session, 1), QStringLiteral("answer"));
    EXPECT_TRUE(m_backend.requests.empty());
}

TEST_F(ChatSessionTest, ASessionOutlivingTheChat_SurvivesTheSignalsOfTheRunner)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    m_session->setConversationId(idText(conversation));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));
    m_chat.reset();

    emit m_runner->turnStarted(conversation.id());
    emit m_runner->partialText(conversation.id(), QStringLiteral("part"));
    emit m_runner->stepStarted(conversation.id(), delegationStep(chat::StepStatus::Running));
    emit m_runner->turnFinished(conversation.id());
    QTest::qWait(100);

    EXPECT_FALSE(m_session->busy());
    EXPECT_TRUE(m_session->activity().isEmpty());
}

TEST_F(ChatSessionTest, ASessionOutlivingTheChatAndTheRunner_StaysUsable)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));
    m_session->setConversationId(idText(conversation));
    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));

    m_chat.reset();
    m_runner.reset();

    m_session->send(QStringLiteral("hello"));
    m_session->cancel();
    m_session->retry();
    m_session->setConversationId(QString());
    EXPECT_FALSE(m_session->busy());
    EXPECT_EQ(rowsOf(*m_session), 0);
}

TEST_F(ChatSessionTest, ALoadFinishingAfterTheChatIsGone_EndsIdle)
{
    const chat::Conversation conversation
        = conversationWithQuestionAndAnswer(QStringLiteral("Topic"));

    m_session->setConversationId(idText(conversation));
    m_chat.reset();

    ASSERT_TRUE(waitUntil([&] { return rowsOf(*m_session) == 2; }));
    EXPECT_FALSE(m_session->busy());
}

TEST_F(ChatSessionTest, ACreationFinishingAfterTheChatIsGone_EndsIdleWithoutATurn)
{
    m_session->send(QStringLiteral("question"));
    ASSERT_TRUE(m_session->busy());

    m_chat.reset();

    ASSERT_TRUE(waitUntil([&] { return !m_session->busy(); }));
    EXPECT_TRUE(m_backend.requests.empty());
}

TEST_F(ChatSessionTest, ACreationFinishingAfterASwitch_KeepsTheShownConversationAndRunsTheTurn)
{
    const chat::Conversation other = conversationWithQuestionAndAnswer(QStringLiteral("Other"));

    m_session->send(QStringLiteral("question"));
    m_session->setConversationId(idText(other));

    ASSERT_TRUE(waitForTurns(1));
    ASSERT_TRUE(waitUntil(
        [&] { return rowsOf(*m_session) == 2 && m_session->title() == QStringLiteral("Other"); }));
    const std::vector<chat::Conversation> conversations = awaitOk(m_store->conversations());
    ASSERT_EQ(conversations.size(), 2u);
    const chat::Conversation created
        = conversations.front().id() == other.id() ? conversations.back() : conversations.front();
    EXPECT_TRUE(m_runner->isBusy(created.id()));
    EXPECT_EQ(m_session->conversationId(), idText(other));
    EXPECT_FALSE(m_session->busy());

    m_backend.latest->complete(QStringLiteral("answer"));
    ASSERT_TRUE(waitUntil([&] { return !m_runner->isBusy(created.id()); }));
    QTest::qWait(100);
    EXPECT_EQ(m_session->conversationId(), idText(other));
    EXPECT_EQ(rowsOf(*m_session), 2);
    EXPECT_FALSE(m_session->busy());
}
