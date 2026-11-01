#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QTest>

#include "agent/claudecli.h"
#include "chat/application/agentcatalog.h"
#include "chat/application/transcript.h"
#include "chat/application/turnrunner.h"
#include "chatfixture.h"
#include "fakeagentbackend.h"

using namespace chat;
using namespace chat_test;

namespace
{

constexpr int kWaitMilliseconds = 5000;

const QString kQuestion = QStringLiteral("question");

BackendSession sessionAt(const QString& id, int start = 0) { return BackendSession { id, start }; }

QStringList texts(const std::vector<chat::Message>& messages)
{
    QStringList list;
    for (const chat::Message& message : messages)
    {
        list.append(message.text());
    }
    return list;
}

}

class TurnRunnerTest : public ChatFixture
{
protected:
    void SetUp() override
    {
        ChatFixture::SetUp();
        createRunner();
    }

    void TearDown() override
    {
        m_runner.reset();
        ChatFixture::TearDown();
    }

    void createRunner() { m_runner = std::make_unique<TurnRunner>(*m_store, m_backend, m_catalog); }

    template <typename Predicate> bool waitUntil(Predicate predicate)
    {
        return QTest::qWaitFor(predicate, kWaitMilliseconds);
    }

    FakeTurn* awaitStart(size_t startedBefore)
    {
        EXPECT_TRUE(waitUntil([&] { return m_backend.requests.size() > startedBefore; }));
        return m_backend.latest.data();
    }

    FakeTurn* startTurn(const QUuid& id, const QString& text = kQuestion)
    {
        const size_t before = m_backend.requests.size();
        EXPECT_TRUE(m_runner->send(id, text));
        return awaitStart(before);
    }

    FakeTurn* retryTurn(const QUuid& id)
    {
        const size_t before = m_backend.requests.size();
        EXPECT_TRUE(m_runner->retry(id));
        return awaitStart(before);
    }

    void settle(const QUuid& id)
    {
        EXPECT_TRUE(waitUntil([&] { return !m_runner->isBusy(id); }));
    }

    void completeTurn(const QUuid& id, FakeTurn* turn,
                      const QString& answer = QStringLiteral("answer"))
    {
        ASSERT_NE(turn, nullptr);
        turn->complete(answer);
        settle(id);
    }

    void failTurn(const QUuid& id, FakeTurn* turn, const QString& token = claudecli::kNetworkError)
    {
        ASSERT_NE(turn, nullptr);
        turn->fail(token);
        settle(id);
    }

    void cancelTurn(const QUuid& id, FakeTurn* turn)
    {
        ASSERT_NE(turn, nullptr);
        m_runner->cancel(id);
        turn->fail(turn_error::kCancelled);
        settle(id);
    }

    void fillHistory(const QUuid& conversationId, int count)
    {
        for (int index = 0; index < count; ++index)
        {
            appendMessage(conversationId, index % 2 == 0 ? Role::User : Role::Assistant,
                          QStringLiteral("m%1").arg(index));
        }
    }

    Conversation conversationOverTheCap(const BackendSession& session)
    {
        const Conversation conversation = newConversation();
        fillHistory(conversation.id(), 45);
        const chat::Message answer = appendRunningAnswer(conversation.id());
        awaitOk(m_store->finishMessage(answer.id(), MessageStatus::Complete, QStringLiteral("x"),
                                       QString(), session));
        return conversation;
    }

    chat::Message lastMessage(const QUuid& conversationId)
    {
        return messagesOf(conversationId).back();
    }

    AgentCatalog m_catalog;
    FakeBackend m_backend;
    std::unique_ptr<TurnRunner> m_runner;
};

TEST_F(TurnRunnerTest, Send_BlankText_IsRejectedAndStoresNothing)
{
    const Conversation conversation = newConversation();

    EXPECT_FALSE(m_runner->send(conversation.id(), QStringLiteral("  \n")));

    EXPECT_FALSE(m_runner->isBusy(conversation.id()));
    EXPECT_TRUE(messagesOf(conversation.id()).empty());
    EXPECT_TRUE(m_backend.requests.empty());
}

TEST_F(TurnRunnerTest, Send_StoresTheQuestionAndARunningAnswerThenStartsTheBackend)
{
    const Conversation conversation = newConversation();
    QSignalSpy started(m_runner.get(), &TurnRunner::turnStarted);

    FakeTurn* turn = startTurn(conversation.id(), QStringLiteral("What is light?"));

    ASSERT_NE(turn, nullptr);
    EXPECT_TRUE(m_runner->isBusy(conversation.id()));
    EXPECT_EQ(started.count(), 1);
    ASSERT_EQ(m_backend.requests.size(), 1u);
    EXPECT_EQ(m_backend.requests.front().conversationId, conversation.id());

    const std::vector<chat::Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.size(), 2u);
    EXPECT_EQ(stored[0].role(), Role::User);
    EXPECT_EQ(stored[0].text(), QStringLiteral("What is light?"));
    EXPECT_EQ(stored[0].status(), MessageStatus::Complete);
    EXPECT_EQ(stored[1].role(), Role::Assistant);
    EXPECT_EQ(stored[1].status(), MessageStatus::Running);

    completeTurn(conversation.id(), turn);
}

TEST_F(TurnRunnerTest, Send_PassesTheProfileAndTheContextToTheBackend)
{
    const Conversation conversation = newConversation();
    m_runner->setProfile(TurnProfile { QStringLiteral("Be brief."), QStringLiteral("model-x"),
                                       QStringLiteral("high") });
    const ChatContext context { QStringLiteral("lesson"), QStringLiteral("Optics"),
                                QJsonObject { { QStringLiteral("page"), 3 } } };
    const size_t before = m_backend.requests.size();

    EXPECT_TRUE(m_runner->send(conversation.id(), kQuestion, context));
    FakeTurn* turn = awaitStart(before);

    const TurnRequest& request = m_backend.requests.front();
    EXPECT_EQ(request.mainInstructions, QStringLiteral("Be brief."));
    EXPECT_EQ(request.model, QStringLiteral("model-x"));
    EXPECT_EQ(request.reasoningEffort, QStringLiteral("high"));
    ASSERT_TRUE(request.context.has_value());
    EXPECT_EQ(*request.context, context);
    const std::vector<chat::Message> stored = messagesOf(conversation.id());
    ASSERT_FALSE(stored.empty());
    ASSERT_TRUE(stored.front().context().has_value());
    EXPECT_EQ(*stored.front().context(), context);

    completeTurn(conversation.id(), turn);
}

TEST_F(TurnRunnerTest, Send_WhileTheConversationIsBusy_IsRejected)
{
    const Conversation conversation = newConversation();
    FakeTurn* turn = startTurn(conversation.id());

    EXPECT_FALSE(m_runner->send(conversation.id(), QStringLiteral("again")));
    EXPECT_FALSE(m_runner->retry(conversation.id()));

    EXPECT_EQ(m_backend.requests.size(), 1u);
    EXPECT_EQ(messagesOf(conversation.id()).size(), 2u);
    completeTurn(conversation.id(), turn);
    EXPECT_FALSE(m_runner->isBusy(conversation.id()));
}

TEST_F(TurnRunnerTest, Send_ToDifferentConversations_RunsThemSideBySide)
{
    const Conversation first = newConversation();
    const Conversation second = newConversation();

    FakeTurn* firstTurn = startTurn(first.id());
    FakeTurn* secondTurn = startTurn(second.id());

    ASSERT_NE(firstTurn, secondTurn);
    EXPECT_TRUE(m_runner->isBusy(first.id()));
    EXPECT_TRUE(m_runner->isBusy(second.id()));
    completeTurn(second.id(), secondTurn);
    EXPECT_TRUE(m_runner->isBusy(first.id()));
    completeTurn(first.id(), firstTurn);
}

TEST_F(TurnRunnerTest, Finished_CompletesTheAnswerWithItsText)
{
    const Conversation conversation = newConversation();
    QSignalSpy finished(m_runner.get(), &TurnRunner::turnFinished);
    FakeTurn* turn = startTurn(conversation.id());

    completeTurn(conversation.id(), turn, QStringLiteral("Light is a wave."));

    const chat::Message answer = lastMessage(conversation.id());
    EXPECT_EQ(answer.status(), MessageStatus::Complete);
    EXPECT_EQ(answer.text(), QStringLiteral("Light is a wave."));
    EXPECT_TRUE(answer.error().isEmpty());
    EXPECT_EQ(finished.count(), 1);
    EXPECT_EQ(finished.at(0).at(0).toUuid(), conversation.id());
}

TEST_F(TurnRunnerTest, SecondTurn_SeesTheFirstAnswerInItsTranscript)
{
    const Conversation conversation = newConversation();
    completeTurn(conversation.id(), startTurn(conversation.id(), QStringLiteral("q1")),
                 QStringLiteral("a1"));

    completeTurn(conversation.id(), startTurn(conversation.id(), QStringLiteral("q2")),
                 QStringLiteral("a2"));

    ASSERT_EQ(m_backend.requests.size(), 2u);
    EXPECT_EQ(texts(m_backend.requests[1].transcript),
              (QStringList { QStringLiteral("q1"), QStringLiteral("a1"), QStringLiteral("q2") }));
    EXPECT_EQ(messagesOf(conversation.id()).size(), 4u);
}

TEST_F(TurnRunnerTest, Transcript_LeavesOutFailedAnswersAndTheRunningOne)
{
    const Conversation conversation = newConversation();
    appendMessage(conversation.id(), Role::User, QStringLiteral("q1"));
    appendMessage(conversation.id(), Role::Assistant, QStringLiteral("a1"));
    appendMessage(conversation.id(), Role::Assistant, QStringLiteral("broken"),
                  MessageStatus::Failed);

    FakeTurn* turn = startTurn(conversation.id(), QStringLiteral("q2"));

    EXPECT_EQ(texts(m_backend.requests.front().transcript),
              (QStringList { QStringLiteral("q1"), QStringLiteral("a1"), QStringLiteral("q2") }));
    completeTurn(conversation.id(), turn);
}

TEST_F(TurnRunnerTest, Steps_AreForwardedToTheViewAndStoredWithTheAnswer)
{
    const Conversation conversation = newConversation();
    std::vector<Step> started;
    std::vector<Step> finished;
    QObject::connect(m_runner.get(), &TurnRunner::stepStarted, m_runner.get(),
                     [&started](const QUuid&, const Step& step) { started.push_back(step); });
    QObject::connect(m_runner.get(), &TurnRunner::stepFinished, m_runner.get(),
                     [&finished](const QUuid&, const Step& step) { finished.push_back(step); });
    FakeTurn* turn = startTurn(conversation.id());
    ASSERT_NE(turn, nullptr);

    Step step = toolStep(StepStatus::Running);
    turn->reportStepStarted(step);
    step.status = StepStatus::Ok;
    step.output = QJsonObject { { QStringLiteral("count"), 2 } };
    turn->reportStepFinished(step);
    completeTurn(conversation.id(), turn);

    ASSERT_EQ(started.size(), 1u);
    EXPECT_EQ(started.front().status, StepStatus::Running);
    ASSERT_EQ(finished.size(), 1u);
    EXPECT_EQ(finished.front(), step);
    const chat::Message answer = lastMessage(conversation.id());
    ASSERT_EQ(answer.steps().size(), 1u);
    EXPECT_EQ(answer.steps().front(), step);
}

TEST_F(TurnRunnerTest, PartialText_IsForwardedToTheViewWithTheConversationId)
{
    const Conversation conversation = newConversation();
    FakeTurn* turn = startTurn(conversation.id());
    ASSERT_NE(turn, nullptr);
    QSignalSpy spy(m_runner.get(), &TurnRunner::partialText);

    turn->reportPartialText(QStringLiteral("Hel"));
    turn->reportPartialText(QStringLiteral("Hello"));
    completeTurn(conversation.id(), turn, QStringLiteral("Hello"));

    ASSERT_EQ(spy.count(), 2);
    EXPECT_EQ(spy.at(0).at(0).toUuid(), conversation.id());
    EXPECT_EQ(spy.at(0).at(1).toString(), QStringLiteral("Hel"));
    EXPECT_EQ(spy.at(1).at(1).toString(), QStringLiteral("Hello"));
}

TEST_F(TurnRunnerTest, Failed_KeepsTheAnswerInHistoryAsFailedWithTheErrorToken)
{
    const Conversation conversation = newConversation();
    FakeTurn* turn = startTurn(conversation.id());

    failTurn(conversation.id(), turn, claudecli::kNetworkError);

    const std::vector<chat::Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.size(), 2u);
    EXPECT_EQ(stored[1].status(), MessageStatus::Failed);
    EXPECT_EQ(stored[1].error(), claudecli::kNetworkError);
    EXPECT_TRUE(stored[1].text().isEmpty());
}

TEST_F(TurnRunnerTest, BackendThatCannotStart_FailsTheAnswerAsUnavailable)
{
    const Conversation conversation = newConversation();
    m_backend.refuseToStart = true;

    EXPECT_TRUE(m_runner->send(conversation.id(), kQuestion));
    settle(conversation.id());

    const chat::Message answer = lastMessage(conversation.id());
    EXPECT_EQ(answer.status(), MessageStatus::Failed);
    EXPECT_EQ(answer.error(), turn_error::kUnavailable);
    EXPECT_EQ(m_backend.requests.size(), 1u);
}

TEST_F(TurnRunnerTest, Cancel_AsksTheBackendAndKeepsTheAnswerAsCancelled)
{
    const Conversation conversation = newConversation();
    FakeTurn* turn = startTurn(conversation.id());
    ASSERT_NE(turn, nullptr);

    m_runner->cancel(conversation.id());

    EXPECT_EQ(turn->cancelCalls, 1);
    EXPECT_TRUE(m_runner->isBusy(conversation.id()));
    turn->fail(turn_error::kCancelled);
    settle(conversation.id());
    const chat::Message answer = lastMessage(conversation.id());
    EXPECT_EQ(answer.status(), MessageStatus::Cancelled);
    EXPECT_EQ(answer.error(), turn_error::kCancelled);
}

TEST_F(TurnRunnerTest, Cancel_BeforeTheBackendStarted_ConcludesWithoutStartingIt)
{
    const Conversation conversation = newConversation();

    EXPECT_TRUE(m_runner->send(conversation.id(), kQuestion));
    m_runner->cancel(conversation.id());
    settle(conversation.id());

    EXPECT_TRUE(m_backend.requests.empty());
    const std::vector<chat::Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.size(), 2u);
    EXPECT_EQ(stored[1].status(), MessageStatus::Cancelled);
    EXPECT_EQ(stored[1].error(), turn_error::kCancelled);
}

TEST_F(TurnRunnerTest, Cancel_WhenNothingRuns_DoesNothing)
{
    const Conversation conversation = newConversation();

    m_runner->cancel(conversation.id());
    m_runner->cancel(QUuid::createUuid());

    EXPECT_FALSE(m_runner->isBusy(conversation.id()));
}

TEST_F(TurnRunnerTest, Retry_AfterAFailure_RunsTheSameAnswerAgain)
{
    const Conversation conversation = newConversation();
    failTurn(conversation.id(), startTurn(conversation.id()));
    const chat::Message failed = lastMessage(conversation.id());

    FakeTurn* turn = retryTurn(conversation.id());

    const chat::Message running = lastMessage(conversation.id());
    EXPECT_EQ(running.id(), failed.id());
    EXPECT_EQ(running.status(), MessageStatus::Running);
    EXPECT_TRUE(running.error().isEmpty());
    EXPECT_EQ(texts(m_backend.requests[1].transcript), QStringList { kQuestion });

    completeTurn(conversation.id(), turn, QStringLiteral("recovered"));
    const std::vector<chat::Message> stored = messagesOf(conversation.id());
    ASSERT_EQ(stored.size(), 2u);
    EXPECT_EQ(stored[1].id(), failed.id());
    EXPECT_EQ(stored[1].status(), MessageStatus::Complete);
    EXPECT_EQ(stored[1].text(), QStringLiteral("recovered"));
}

TEST_F(TurnRunnerTest, Retry_AfterACancel_RunsTheSameAnswerAgain)
{
    const Conversation conversation = newConversation();
    cancelTurn(conversation.id(), startTurn(conversation.id()));

    FakeTurn* turn = retryTurn(conversation.id());

    EXPECT_EQ(lastMessage(conversation.id()).status(), MessageStatus::Running);
    completeTurn(conversation.id(), turn);
    EXPECT_EQ(lastMessage(conversation.id()).status(), MessageStatus::Complete);
}

TEST_F(TurnRunnerTest, Retry_WhenTheLastAnswerIsComplete_StartsNothing)
{
    const Conversation conversation = newConversation();
    completeTurn(conversation.id(), startTurn(conversation.id()));

    EXPECT_TRUE(m_runner->retry(conversation.id()));
    settle(conversation.id());

    EXPECT_EQ(m_backend.requests.size(), 1u);
    EXPECT_EQ(lastMessage(conversation.id()).status(), MessageStatus::Complete);
}

TEST_F(TurnRunnerTest, Retry_OnAnEmptyConversation_StartsNothing)
{
    const Conversation conversation = newConversation();

    EXPECT_TRUE(m_runner->retry(conversation.id()));
    settle(conversation.id());

    EXPECT_TRUE(m_backend.requests.empty());
}

TEST_F(TurnRunnerTest, Session_UnderTheCap_IsPassedToTheBackend)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 0));

    FakeTurn* turn = startTurn(conversation.id(), QStringLiteral("next"));

    ASSERT_EQ(m_backend.requests.size(), 1u);
    EXPECT_EQ(m_backend.requests.front().backendSession, QStringLiteral("s1"));
    EXPECT_EQ(texts(m_backend.requests.front().transcript),
              (QStringList { QStringLiteral("question"), QStringLiteral("answer"),
                             QStringLiteral("next") }));
    completeTurn(conversation.id(), turn);
}

TEST_F(TurnRunnerTest, Session_OverTheCap_StartsAfreshWithHalfTheCapAsTheSeed)
{
    const Conversation conversation = conversationOverTheCap(sessionAt("s1", 0));

    FakeTurn* turn = startTurn(conversation.id(), QStringLiteral("next"));

    ASSERT_EQ(m_backend.requests.size(), 1u);
    const TurnRequest& request = m_backend.requests.front();
    EXPECT_TRUE(request.backendSession.isEmpty());
    ASSERT_EQ(request.transcript.size(), 21u);
    EXPECT_EQ(request.transcript.front().text(), kOmittedMarker);
    EXPECT_EQ(request.transcript[1].position(), 27);
    EXPECT_EQ(request.transcript.back().text(), QStringLiteral("next"));
    completeTurn(conversation.id(), turn);
}

TEST_F(TurnRunnerTest, Session_OverTheCap_ForgetsTheOldSessionAndStoresTheNewOneWithTheSeedStart)
{
    const Conversation conversation = conversationOverTheCap(sessionAt("s1", 0));
    FakeTurn* turn = startTurn(conversation.id());
    ASSERT_NE(turn, nullptr);

    turn->reportSession(QStringLiteral("s2"));
    completeTurn(conversation.id(), turn);

    EXPECT_EQ(m_backend.forgotten, QStringList { QStringLiteral("s1") });
    ASSERT_TRUE(sessionOf(conversation.id()).has_value());
    EXPECT_EQ(*sessionOf(conversation.id()), sessionAt("s2", 27));
}

TEST_F(TurnRunnerTest, Session_FinishedAfterASeed_StoresTheSessionWithTheSeedStart)
{
    const Conversation conversation = newConversation();
    appendMessage(conversation.id(), Role::User, QStringLiteral("q1"));
    appendMessage(conversation.id(), Role::Assistant, QStringLiteral("a1"));
    FakeTurn* turn = startTurn(conversation.id());
    ASSERT_NE(turn, nullptr);
    EXPECT_TRUE(m_backend.requests.front().backendSession.isEmpty());

    turn->reportSession(QStringLiteral("s1"));
    completeTurn(conversation.id(), turn);

    ASSERT_TRUE(sessionOf(conversation.id()).has_value());
    EXPECT_EQ(*sessionOf(conversation.id()), sessionAt("s1", 0));
    EXPECT_TRUE(m_backend.forgotten.isEmpty());
}

TEST_F(TurnRunnerTest, Session_BackendThatReportsNone_StoresNone)
{
    const Conversation conversation = newConversation();

    completeTurn(conversation.id(), startTurn(conversation.id()));

    EXPECT_FALSE(sessionOf(conversation.id()).has_value());
    EXPECT_TRUE(m_backend.forgotten.isEmpty());
}

TEST_F(TurnRunnerTest, Session_FinishedAfterAResume_KeepsTheStart)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 1));
    FakeTurn* turn = startTurn(conversation.id(), QStringLiteral("next"));
    ASSERT_NE(turn, nullptr);
    EXPECT_EQ(m_backend.requests.front().backendSession, QStringLiteral("s1"));
    EXPECT_EQ(texts(m_backend.requests.front().transcript),
              (QStringList { kOmittedMarker, QStringLiteral("answer"), QStringLiteral("next") }));

    completeTurn(conversation.id(), turn);

    ASSERT_TRUE(sessionOf(conversation.id()).has_value());
    EXPECT_EQ(*sessionOf(conversation.id()), sessionAt("s1", 1));
    EXPECT_TRUE(m_backend.forgotten.isEmpty());
}

TEST_F(TurnRunnerTest, Session_ResumedTurnThatReportsAnotherId_KeepsTheStartAndForgetsTheOldId)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 1));
    FakeTurn* turn = startTurn(conversation.id());
    ASSERT_NE(turn, nullptr);

    turn->reportSession(QStringLiteral("s1b"));
    completeTurn(conversation.id(), turn);

    ASSERT_TRUE(sessionOf(conversation.id()).has_value());
    EXPECT_EQ(*sessionOf(conversation.id()), sessionAt("s1b", 1));
    EXPECT_EQ(m_backend.forgotten, QStringList { QStringLiteral("s1") });
}

TEST_F(TurnRunnerTest, Session_FailedTurn_ClearsTheSessionAndForgetsIt)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 0));

    failTurn(conversation.id(), startTurn(conversation.id()));

    EXPECT_FALSE(sessionOf(conversation.id()).has_value());
    EXPECT_EQ(m_backend.forgotten, QStringList { QStringLiteral("s1") });
}

TEST_F(TurnRunnerTest, Session_FailedTurnThatReportedANewSession_ForgetsBothIds)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 0));
    FakeTurn* turn = startTurn(conversation.id());
    ASSERT_NE(turn, nullptr);

    turn->reportSession(QStringLiteral("s2"));
    failTurn(conversation.id(), turn);

    EXPECT_FALSE(sessionOf(conversation.id()).has_value());
    QStringList forgotten = m_backend.forgotten;
    forgotten.sort();
    EXPECT_EQ(forgotten, (QStringList { QStringLiteral("s1"), QStringLiteral("s2") }));
}

TEST_F(TurnRunnerTest, Session_FinishThatCannotBeSaved_ForgetsEveryKnownId)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s0", 0));
    FakeTurn* turn = startTurn(conversation.id());
    ASSERT_NE(turn, nullptr);
    awaitOk(m_store->remove(conversation.id()));

    turn->reportSession(QStringLiteral("s1"));
    completeTurn(conversation.id(), turn);

    QStringList forgotten = m_backend.forgotten;
    forgotten.sort();
    EXPECT_EQ(forgotten, (QStringList { QStringLiteral("s0"), QStringLiteral("s1") }));
}

TEST_F(TurnRunnerTest, Session_CancelledTurn_ClearsTheSessionAndForgetsIt)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 0));

    cancelTurn(conversation.id(), startTurn(conversation.id()));

    EXPECT_FALSE(sessionOf(conversation.id()).has_value());
    EXPECT_EQ(m_backend.forgotten, QStringList { QStringLiteral("s1") });
}

TEST_F(TurnRunnerTest, Session_CancelledBeforeTheBackendStarted_ClearsAndForgetsTheSession)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 0));

    EXPECT_TRUE(m_runner->send(conversation.id(), kQuestion));
    m_runner->cancel(conversation.id());
    settle(conversation.id());

    EXPECT_FALSE(sessionOf(conversation.id()).has_value());
    EXPECT_EQ(m_backend.forgotten, QStringList { QStringLiteral("s1") });
}

TEST_F(TurnRunnerTest, Session_FailureOrCancelWithoutASession_ForgetsNothing)
{
    const Conversation failing = newConversation();
    const Conversation cancelled = newConversation();

    failTurn(failing.id(), startTurn(failing.id()));
    cancelTurn(cancelled.id(), startTurn(cancelled.id()));

    EXPECT_TRUE(m_backend.forgotten.isEmpty());
}

TEST_F(TurnRunnerTest, Retry_AfterAFailedResume_StartsAfreshFromTheTranscript)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 0));
    failTurn(conversation.id(), startTurn(conversation.id(), QStringLiteral("next")));

    FakeTurn* turn = retryTurn(conversation.id());

    ASSERT_EQ(m_backend.requests.size(), 2u);
    EXPECT_TRUE(m_backend.requests[1].backendSession.isEmpty());
    EXPECT_EQ(texts(m_backend.requests[1].transcript),
              (QStringList { QStringLiteral("question"), QStringLiteral("answer"),
                             QStringLiteral("next") }));
    completeTurn(conversation.id(), turn);
}

TEST_F(TurnRunnerTest, Remove_AnIdleConversation_DeletesItAndForgetsItsSession)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 0));
    QSignalSpy removed(m_runner.get(), &TurnRunner::conversationRemoved);

    m_runner->remove(conversation.id());

    EXPECT_TRUE(m_runner->isBusy(conversation.id()));
    ASSERT_TRUE(waitUntil([&] { return removed.count() == 1; }));
    EXPECT_EQ(removed.at(0).at(0).toUuid(), conversation.id());
    EXPECT_FALSE(m_runner->isBusy(conversation.id()));
    EXPECT_TRUE(await(m_store->conversation(conversation.id())).isFailure());
    EXPECT_EQ(m_backend.forgotten, QStringList { QStringLiteral("s1") });
}

TEST_F(TurnRunnerTest, Remove_AConversationWithoutASession_ForgetsNothing)
{
    const Conversation conversation = newConversation();
    QSignalSpy removed(m_runner.get(), &TurnRunner::conversationRemoved);

    m_runner->remove(conversation.id());

    ASSERT_TRUE(waitUntil([&] { return removed.count() == 1; }));
    EXPECT_TRUE(m_backend.forgotten.isEmpty());
}

TEST_F(TurnRunnerTest, Remove_WhileATurnRuns_CancelsItBeforeTheRowIsDeleted)
{
    const Conversation conversation
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 0));
    FakeTurn* turn = startTurn(conversation.id());
    ASSERT_NE(turn, nullptr);
    QSignalSpy removed(m_runner.get(), &TurnRunner::conversationRemoved);

    m_runner->remove(conversation.id());

    EXPECT_EQ(turn->cancelCalls, 1);
    EXPECT_TRUE(m_runner->isBusy(conversation.id()));
    EXPECT_TRUE(await(m_store->conversation(conversation.id())).isSuccess());
    EXPECT_EQ(removed.count(), 0);
    EXPECT_FALSE(m_runner->send(conversation.id(), QStringLiteral("late")));

    turn->fail(turn_error::kCancelled);
    ASSERT_TRUE(waitUntil([&] { return removed.count() == 1; }));
    EXPECT_TRUE(await(m_store->conversation(conversation.id())).isFailure());
    EXPECT_TRUE(messagesOf(conversation.id()).empty());
    EXPECT_EQ(m_backend.forgotten, QStringList { QStringLiteral("s1") });
}

TEST_F(TurnRunnerTest, Remove_BeforeTheBackendStarted_NeverStartsItAndDeletesTheConversation)
{
    const Conversation conversation = newConversation();
    QSignalSpy removed(m_runner.get(), &TurnRunner::conversationRemoved);

    EXPECT_TRUE(m_runner->send(conversation.id(), kQuestion));
    m_runner->remove(conversation.id());

    ASSERT_TRUE(waitUntil([&] { return removed.count() == 1; }));
    EXPECT_TRUE(m_backend.requests.empty());
    EXPECT_TRUE(await(m_store->conversation(conversation.id())).isFailure());
}

TEST_F(TurnRunnerTest, ForgetSessions_ForwardsEveryIdToTheBackend)
{
    m_runner->forgetSessions({ QStringLiteral("a"), QStringLiteral("b") });

    EXPECT_EQ(m_backend.forgotten, (QStringList { QStringLiteral("a"), QStringLiteral("b") }));
}

TEST_F(TurnRunnerTest, ForgetSessions_WithTheSessionsDroppedOnOpen_ForgetsTheInterruptedOne)
{
    const Conversation interrupted
        = conversationWithSession(QStringLiteral("A"), sessionAt("s1", 0));
    appendRunningAnswer(interrupted.id());
    conversationWithSession(QStringLiteral("B"), sessionAt("s2", 0));
    m_runner.reset();
    reopenStorage();
    createRunner();

    m_runner->forgetSessions(m_storage->droppedSessions());

    EXPECT_EQ(m_backend.forgotten, QStringList { QStringLiteral("s1") });
    EXPECT_FALSE(sessionOf(interrupted.id()).has_value());
}
