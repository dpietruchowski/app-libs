#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <QUuid>
#include <limits>
#include <memory>
#include <vector>

#include "chat/application/chatagent.h"
#include "fakeclient.h"

using chat::AgentCatalog;
using chat::AppAgent;
using chat::ChatAgent;
using chat::Role;
using chat::Step;
using chat::StepKind;
using chat::StepStatus;
using chat::TurnRequest;
using namespace chat_test;

namespace
{

const QString kMainInstructions = QStringLiteral("You are the main agent.");
const QString kCoursesPrompt = QStringLiteral("You are the courses agent.");
const QString kFactsPrompt = QStringLiteral("You are the facts agent.");
const QString kCoursesDescription = QStringLiteral("Answers questions about courses.");
const QString kTask = QStringLiteral("list them");
const QString kToolResult = QStringLiteral("3 courses");
const QString kReport = QStringLiteral("You have 3 courses.");
const QString kAnswer = QStringLiteral("You have three courses.");
const QString kMainSession = QStringLiteral("main-session");
const QString kQuestion = QStringLiteral("What do I have?");
const QString kContextLine = QStringLiteral("\n[context: Stopwatch {\"id\":\"stopwatch\"}]");

chat::ChatContext stopwatchContext()
{
    return chat::ChatContext { QStringLiteral("app"), QStringLiteral("Stopwatch"),
                               QJsonObject {
                                   { QStringLiteral("id"), QStringLiteral("stopwatch") } } };
}

const QString kListCourses = QStringLiteral("@brief list_courses\n@details Lists the courses.\n"
                                            "@param limit (integer) How many courses to list.");
const QString kDeleteFact = QStringLiteral("@brief delete_fact\n@details Deletes a fact.\n"
                                           "@param id (string) The fact id.");

chat::Message message(Role role, const QString& text)
{
    chat::Message::Data data;
    data.role = role;
    data.text = text;
    return chat::Message::create(data, QDateTime::currentDateTimeUtc());
}

QString statusName(StepStatus status)
{
    switch (status)
    {
        case StepStatus::Running:
            return QStringLiteral("Running");
        case StepStatus::Ok:
            return QStringLiteral("Ok");
        case StepStatus::Failed:
            return QStringLiteral("Failed");
    }
    return QString();
}

const QString kCompactionRequest = QStringLiteral("Write your summary now.");
const QString kSummary
    = QStringLiteral("App: Courses (courses)\nDone:\n1. listed them\nProblems:\n- none");
const QString kSummaryMessage = QStringLiteral("Summary:\n") + kSummary;
constexpr int kMaxInputTokens = 100000;
constexpr std::chrono::minutes kMaxAge { 55 };

chat::ContextPolicy keepPolicy()
{
    return chat::ContextPolicy {
        .maxInputTokens = kMaxInputTokens,
        .maxAge = kMaxAge,
        .compactionRequest = kCompactionRequest,
        .compactedContext =
            [](const QString& reply)
        {
            return reply.startsWith(QStringLiteral("App:")) ? QStringLiteral("Summary:\n") + reply
                                                            : QString();
        },
    };
}

Completion usingTokens(int inputTokens, const QString& text = kReport)
{
    Completion completion = contentCompletion(text);
    completion.usage.context_tokens = inputTokens;
    return completion;
}

QString describe(const Step& step)
{
    QString text = (step.kind == StepKind::Delegation ? QStringLiteral("Delegation")
                                                      : QStringLiteral("Tool"))
        + QLatin1Char(' ') + step.agent;
    if (!step.tool.isEmpty())
    {
        text += QLatin1Char(' ') + step.tool;
    }
    return text;
}

}

class ChatAgentTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_backend = std::make_unique<ChatAgent>(
            [this](const ClaudeSession& session) { return makeClient(session); },
            [this] { return m_available; }, [this](const QString& id) { m_forgotten.append(id); },
            [this] { return m_now; });
        m_catalog.add(coursesAgent());
        m_catalog.add(factsAgent());
    }

    void TearDown() override { m_turn.reset(); }

    AppAgent coursesAgent()
    {
        ::Agent agent(QStringLiteral("model"), kCoursesPrompt);
        agent.addTool(kListCourses, appTool());
        return AppAgent { QStringLiteral("courses"), QStringLiteral("Courses"), kCoursesDescription,
                          agent, [this] { return nextFollowUp(); } };
    }

    void useCoursesAgent(const QString& model, const QString& reasoningEffort)
    {
        AppAgent entry = coursesAgent();
        entry.config = ModelConfig { .model = model, .reasoningEffort = reasoningEffort };
        m_catalog = AgentCatalog();
        m_catalog.add(std::move(entry));
    }

    void limitCoursesAgentTo(int delegations)
    {
        AppAgent entry = coursesAgent();
        entry.maxDelegations = delegations;
        m_catalog = AgentCatalog();
        m_catalog.add(std::move(entry));
        m_catalog.add(factsAgent());
    }

    void delegateAndReport(const QString& key, size_t clientIndex)
    {
        callDelegate(key);
        delegateClient(clientIndex).completeNext(contentCompletion(kReport));
    }

    QString nextFollowUp()
    {
        ++m_followUpCalls;
        return m_followUps.isEmpty() ? QString() : m_followUps.takeFirst();
    }

    AppAgent factsAgent()
    {
        ::Agent agent(QStringLiteral("model"), kFactsPrompt);
        agent.addTool(kDeleteFact, appTool(), ToolEffect::Writes);
        return AppAgent { QStringLiteral("facts"), QStringLiteral("Facts"),
                          QStringLiteral("Answers questions about facts."), agent };
    }

    Tool appTool()
    {
        return [this](QVariantMap arguments, ToolDone done)
        {
            m_toolArguments.push_back(arguments);
            if (m_holdTools)
            {
                m_heldTools.push_back(done);
                return;
            }
            done(kToolResult);
        };
    }

    std::unique_ptr<::Client> makeClient(const ClaudeSession& session)
    {
        const bool isMain = m_startingMain;
        if ((isMain ? m_refuseMain : m_refuseDelegates) || m_clients.size() >= m_clientLimit)
        {
            return nullptr;
        }
        auto state = std::make_shared<ClientState>();
        state->requested = session;
        state->oneShot = true;
        if (isMain)
        {
            state->session = m_reportedSession;
        }
        else
        {
            state->immediate = m_delegateImmediate;
            if (session.keep)
            {
                state->session = session.resume.isEmpty()
                    ? QStringLiteral("delegate-session-%1").arg(++m_delegateSessions)
                    : session.resume;
            }
        }
        m_clients.push_back(state);
        return std::make_unique<ScriptedClient>(state);
    }

    std::unique_ptr<chat::AgentTurn> startTurn(const TurnRequest& turnRequest)
    {
        m_startingMain = true;
        auto turn = m_backend->start(turnRequest, m_catalog);
        m_startingMain = false;
        return turn;
    }

    TurnRequest request(std::vector<chat::Message> transcript = {})
    {
        TurnRequest turnRequest;
        turnRequest.conversationId = m_conversation;
        turnRequest.mainInstructions = kMainInstructions;
        turnRequest.model = QStringLiteral("test-model");
        turnRequest.reasoningEffort = QStringLiteral("low");
        turnRequest.transcript = transcript.empty()
            ? std::vector<chat::Message> { message(Role::User, kQuestion) }
            : std::move(transcript);
        return turnRequest;
    }

    void begin(const TurnRequest& turnRequest)
    {
        m_turn = startTurn(turnRequest);
        ASSERT_NE(m_turn, nullptr);
        record(*m_turn);
    }

    void beginAndAsk()
    {
        begin(request());
        settle();
    }

    void settle() { QCoreApplication::processEvents(); }

    void record(chat::AgentTurn& turn)
    {
        QObject::connect(&turn, &chat::AgentTurn::sessionReady, &turn,
                         [this](const QString& id) { m_events.append("session:" + id); });
        QObject::connect(&turn, &chat::AgentTurn::partialText, &turn,
                         [this](const QString& text) { m_events.append("partial:" + text); });
        QObject::connect(&turn, &chat::AgentTurn::stepStarted, &turn,
                         [this](const Step& step)
                         {
                             m_started.push_back(step);
                             m_events.append("start:" + describe(step));
                         });
        QObject::connect(&turn, &chat::AgentTurn::stepFinished, &turn,
                         [this](const Step& step)
                         {
                             m_ended.push_back(step);
                             m_events.append("end:" + describe(step) + " "
                                             + statusName(step.status));
                         });
        QObject::connect(&turn, &chat::AgentTurn::finished, &turn,
                         [this](const QString& answer) { m_events.append("finished:" + answer); });
        QObject::connect(&turn, &chat::AgentTurn::failed, &turn,
                         [this](const QString& token) { m_events.append("failed:" + token); });
    }

    ClientState& mainClient() { return *m_clients.at(0); }
    ClientState& delegateClient(size_t index = 0) { return *m_clients.at(index + 1); }

    void callDelegate(const QString& key, const QString& task = kTask)
    {
        mainClient().completeNext(toolCallCompletion(QStringLiteral("call-") + key, key,
                                                     { { QStringLiteral("task"), task } }));
    }

    QString lastMessageOf(const ClientState& client) const
    {
        return client.held.front().messages.constLast().content;
    }

    TurnRequest requestWithContext(const chat::ChatContext& context)
    {
        TurnRequest turnRequest = request();
        turnRequest.context = context;
        return turnRequest;
    }

    void keepCoursesContext(chat::ContextPolicy policy = keepPolicy(),
                            int maxDelegations = AppAgent::kDefaultMaxDelegations)
    {
        AppAgent entry = coursesAgent();
        entry.keepContext = std::move(policy);
        entry.maxDelegations = maxDelegations;
        m_catalog = AgentCatalog();
        m_catalog.add(std::move(entry));
        m_catalog.add(factsAgent());
    }

    size_t beginTurn(const TurnRequest& turnRequest)
    {
        m_turn.reset();
        const size_t mainIndex = m_clients.size();
        begin(turnRequest);
        settle();
        return mainIndex;
    }

    size_t beginTurn() { return beginTurn(request()); }

    void delegateTo(size_t mainIndex, const QString& task = kTask)
    {
        m_clients.at(mainIndex)->completeNext(
            toolCallCompletion(QStringLiteral("call-courses"), QStringLiteral("courses"),
                               { { QStringLiteral("task"), task } }));
    }

    void report(const Completion& completion) { m_clients.back()->completeNext(completion); }

    void runDelegation(size_t mainIndex, const QString& task, const Completion& completion)
    {
        delegateTo(mainIndex, task);
        report(completion);
    }

    void answer(size_t mainIndex)
    {
        m_clients.at(mainIndex)->completeNext(contentCompletion(kAnswer));
    }

    const ::Messages& sentTo(size_t clientIndex) const
    {
        return m_clients.at(clientIndex)->held.front().messages;
    }

    QStringList sentContents(size_t clientIndex) const
    {
        QStringList contents;
        for (const ::Message& message : sentTo(clientIndex))
        {
            contents.append(message.content);
        }
        return contents;
    }

    QStringList m_followUps;
    int m_followUpCalls { 0 };
    QUuid m_conversation { QUuid::createUuid() };
    QDateTime m_now { QDate(2026, 11, 2), QTime(12, 0), Qt::UTC };
    int m_delegateSessions { 0 };
    bool m_startingMain { false };
    size_t m_clientLimit { std::numeric_limits<size_t>::max() };
    QStringList m_forgotten;
    QStringList m_events;
    std::vector<Step> m_started;
    std::vector<Step> m_ended;
    std::vector<QVariantMap> m_toolArguments;
    std::vector<ToolDone> m_heldTools;
    std::vector<std::shared_ptr<ClientState>> m_clients;
    std::optional<Completion> m_delegateImmediate;
    QString m_reportedSession { kMainSession };
    bool m_available { true };
    bool m_refuseMain { false };
    bool m_refuseDelegates { false };
    bool m_holdTools { false };
    AgentCatalog m_catalog;
    std::unique_ptr<ChatAgent> m_backend;
    std::unique_ptr<chat::AgentTurn> m_turn;
};

TEST_F(ChatAgentTest, Available_FollowsTheAvailabilityCheck)
{
    EXPECT_TRUE(m_backend->available());

    m_available = false;

    EXPECT_FALSE(m_backend->available());
}

TEST_F(ChatAgentTest, Forget_PassesTheSessionToTheForgetCallback)
{
    m_backend->forget(QStringLiteral("s1"));

    EXPECT_EQ(m_forgotten, QStringList { QStringLiteral("s1") });
}

TEST_F(ChatAgentTest, Start_WhenUnavailable_ReturnsNoTurnAndCreatesNoClient)
{
    m_available = false;

    EXPECT_EQ(startTurn(request()), nullptr);

    EXPECT_TRUE(m_clients.empty());
}

TEST_F(ChatAgentTest, Start_WhenTheFactoryGivesNoClient_ReturnsNoTurn)
{
    m_refuseMain = true;

    EXPECT_EQ(startTurn(request()), nullptr);
}

TEST_F(ChatAgentTest, Start_WithoutAnyMessage_ReturnsNoTurn)
{
    TurnRequest empty = request();
    empty.transcript.clear();

    EXPECT_EQ(startTurn(empty), nullptr);
}

TEST_F(ChatAgentTest, Start_AsksTheFactoryForAKeptClientThatResumesTheBackendSession)
{
    TurnRequest resumed = request();
    resumed.backendSession = QStringLiteral("s1");

    begin(resumed);

    ASSERT_EQ(m_clients.size(), 1u);
    EXPECT_EQ(mainClient().requested.resume, QStringLiteral("s1"));
    EXPECT_TRUE(mainClient().requested.keep);
}

TEST_F(ChatAgentTest, Start_SendsTheQuestionOnlyAfterTheEventLoopTurns)
{
    begin(request());

    EXPECT_TRUE(mainClient().held.empty());

    settle();

    EXPECT_EQ(mainClient().held.size(), 1u);
}

TEST_F(ChatAgentTest, Main_GetsTheInstructionsTheHistoryAndTheQuestion)
{
    TurnRequest conversation = request({ message(Role::User, QStringLiteral("q1")),
                                         message(Role::Assistant, QStringLiteral("a1")),
                                         message(Role::User, QStringLiteral("q2")) });

    begin(conversation);
    settle();

    ASSERT_EQ(mainClient().held.size(), 1u);
    const ClientRequest& sent = mainClient().held.front();
    EXPECT_EQ(sent.config.model, QStringLiteral("test-model"));
    EXPECT_EQ(sent.config.reasoningEffort, QStringLiteral("low"));
    ASSERT_EQ(sent.messages.size(), 4);
    EXPECT_EQ(sent.messages.at(0).role, QStringLiteral("system"));
    EXPECT_EQ(sent.messages.at(0).content, kMainInstructions);
    EXPECT_EQ(sent.messages.at(1).role, QStringLiteral("user"));
    EXPECT_EQ(sent.messages.at(1).content, QStringLiteral("q1"));
    EXPECT_EQ(sent.messages.at(2).role, QStringLiteral("assistant"));
    EXPECT_EQ(sent.messages.at(2).content, QStringLiteral("a1"));
    EXPECT_EQ(sent.messages.at(3).role, QStringLiteral("user"));
    EXPECT_EQ(sent.messages.at(3).content, QStringLiteral("q2"));
}

TEST_F(ChatAgentTest, Main_HasOneToolPerAppAgentDescribedByTheCatalog)
{
    beginAndAsk();

    const ToolsMap& tools = mainClient().held.front().tools;
    EXPECT_EQ(tools.keys(), (QStringList { QStringLiteral("courses"), QStringLiteral("facts") }));
    const QJsonObject function
        = tools.value(QStringLiteral("courses")).json.value(QStringLiteral("function")).toObject();
    EXPECT_EQ(function.value(QStringLiteral("description")).toString(), kCoursesDescription);
    const QJsonObject properties = function.value(QStringLiteral("parameters"))
                                       .toObject()
                                       .value(QStringLiteral("properties"))
                                       .toObject();
    EXPECT_EQ(properties.value(QStringLiteral("task"))
                  .toObject()
                  .value(QStringLiteral("type"))
                  .toString(),
              QStringLiteral("string"));
}

TEST_F(ChatAgentTest, Main_OffersTheCatalogAsItWasWhenTheTurnStarted)
{
    begin(request());
    m_catalog.add(AppAgent { QStringLiteral("later"), QStringLiteral("Later"),
                             QStringLiteral("Added too late."),
                             ::Agent(QStringLiteral("model"), QStringLiteral("prompt")) });

    settle();

    EXPECT_EQ(mainClient().held.front().tools.size(), 2);
}

TEST_F(ChatAgentTest, Finished_ReportsTheSessionOfTheClientBeforeTheAnswer)
{
    beginAndAsk();

    mainClient().completeNext(contentCompletion(kAnswer));

    EXPECT_EQ(m_events, (QStringList { "session:" + kMainSession, "finished:" + kAnswer }));
}

TEST_F(ChatAgentTest, Finished_WhenTheClientHasNoSession_ReportsNoSession)
{
    m_reportedSession.clear();
    beginAndAsk();

    mainClient().completeNext(contentCompletion(kAnswer));

    EXPECT_EQ(m_events, QStringList { "finished:" + kAnswer });
}

TEST_F(ChatAgentTest, Failed_ReportsTheErrorTokenOfTheClient)
{
    beginAndAsk();

    mainClient().completeNext(failedCompletion(QStringLiteral("network_error")));

    EXPECT_EQ(m_events, QStringList { QStringLiteral("failed:network_error") });
}

TEST_F(ChatAgentTest, PartialText_IsForwardedWhileTheMainAgentAnswers)
{
    beginAndAsk();

    mainClient().streamNext(QStringLiteral("Hel"));
    mainClient().streamNext(QStringLiteral("Hello"));

    EXPECT_EQ(m_events,
              (QStringList { QStringLiteral("partial:Hel"), QStringLiteral("partial:Hello") }));
}

TEST_F(ChatAgentTest, Delegation_RunsTheAppAgentAloneOnItsOwnClientWithoutASession)
{
    beginAndAsk();

    callDelegate(QStringLiteral("courses"));

    ASSERT_EQ(m_clients.size(), 2u);
    EXPECT_TRUE(delegateClient().requested.resume.isEmpty());
    EXPECT_FALSE(delegateClient().requested.keep);
    ASSERT_EQ(delegateClient().held.size(), 1u);
    const ::Messages& sent = delegateClient().held.front().messages;
    ASSERT_EQ(sent.size(), 2);
    EXPECT_EQ(sent.at(0).role, QStringLiteral("system"));
    EXPECT_EQ(sent.at(0).content, kCoursesPrompt);
    EXPECT_EQ(sent.at(1).role, QStringLiteral("user"));
    EXPECT_EQ(sent.at(1).content, kTask);
    EXPECT_TRUE(delegateClient().held.front().tools.contains(QStringLiteral("list_courses")));
}

TEST_F(ChatAgentTest, Delegation_GivesTheReportBackToTheMainAgentWhichAnswers)
{
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    delegateClient().completeNext(contentCompletion(kReport));

    ASSERT_EQ(mainClient().held.size(), 1u);
    EXPECT_TRUE(lastMessageOf(mainClient()).contains(kReport));
    EXPECT_EQ(mainClient().held.front().messages.constLast().role, QStringLiteral("tool"));
    mainClient().completeNext(contentCompletion(kAnswer));
    EXPECT_EQ(m_events.last(), "finished:" + kAnswer);
}

TEST_F(ChatAgentTest, Steps_ARunWithAToolCall_NestsTheToolUnderTheDelegation)
{
    beginAndAsk();

    callDelegate(QStringLiteral("courses"));
    delegateClient().completeNext(toolCallCompletion(
        QStringLiteral("t1"), QStringLiteral("list_courses"), { { QStringLiteral("limit"), 3 } }));
    delegateClient().completeNext(contentCompletion(kReport));
    mainClient().completeNext(contentCompletion(kAnswer));

    EXPECT_EQ(m_events,
              (QStringList { "start:Delegation courses", "start:Tool courses list_courses",
                             "end:Tool courses list_courses Ok", "end:Delegation courses Ok",
                             "session:" + kMainSession, "finished:" + kAnswer }));
    ASSERT_EQ(m_started.size(), 2u);
    ASSERT_EQ(m_ended.size(), 2u);
    const Step& delegation = m_started.at(0);
    EXPECT_EQ(delegation.kind, StepKind::Delegation);
    EXPECT_FALSE(delegation.parentId.has_value());
    EXPECT_EQ(delegation.input, (QJsonObject { { "task", kTask } }));
    EXPECT_EQ(delegation.effect, ToolEffect::Reads);
    EXPECT_EQ(delegation.status, StepStatus::Running);
    const Step& tool = m_started.at(1);
    EXPECT_EQ(tool.kind, StepKind::Tool);
    EXPECT_EQ(tool.parentId, delegation.id);
    EXPECT_EQ(tool.input, (QJsonObject { { "limit", 3 } }));
    EXPECT_EQ(m_ended.at(0).id, tool.id);
    EXPECT_EQ(m_ended.at(0).output.toString(), kToolResult);
    EXPECT_EQ(m_ended.at(1).id, delegation.id);
    EXPECT_EQ(m_ended.at(1).output.toString(), kReport);
    ASSERT_EQ(m_toolArguments.size(), 1u);
    EXPECT_EQ(m_toolArguments.front().value(QStringLiteral("limit")).toInt(), 3);
}

TEST_F(ChatAgentTest, Steps_AToolThatWrites_IsMarkedWhileItsDelegationReads)
{
    beginAndAsk();

    callDelegate(QStringLiteral("facts"));
    delegateClient().completeNext(
        toolCallCompletion(QStringLiteral("t1"), QStringLiteral("delete_fact"),
                           { { QStringLiteral("id"), QStringLiteral("f1") } }));

    ASSERT_EQ(m_started.size(), 2u);
    EXPECT_EQ(m_started.at(0).effect, ToolEffect::Reads);
    EXPECT_EQ(m_started.at(1).effect, ToolEffect::Writes);
    EXPECT_EQ(m_started.at(1).agent, QStringLiteral("facts"));
}

TEST_F(ChatAgentTest, Steps_TwoDelegationsInARow_RunOneAfterTheOtherWithTheirOwnToolSteps)
{
    beginAndAsk();

    mainClient().completeNext(
        toolCallsCompletion({ toolCall(QStringLiteral("c1"), QStringLiteral("courses"),
                                       { { QStringLiteral("task"), QStringLiteral("a") } }),
                              toolCall(QStringLiteral("c2"), QStringLiteral("facts"),
                                       { { QStringLiteral("task"), QStringLiteral("b") } }) }));
    ASSERT_EQ(m_clients.size(), 2u);
    delegateClient(0).completeNext(contentCompletion(QStringLiteral("A done")));
    ASSERT_EQ(m_clients.size(), 3u);
    delegateClient(1).completeNext(
        toolCallCompletion(QStringLiteral("t1"), QStringLiteral("delete_fact"),
                           { { QStringLiteral("id"), QStringLiteral("f1") } }));
    delegateClient(1).completeNext(contentCompletion(QStringLiteral("B done")));

    EXPECT_EQ(m_events,
              (QStringList { "start:Delegation courses", "end:Delegation courses Ok",
                             "start:Delegation facts", "start:Tool facts delete_fact",
                             "end:Tool facts delete_fact Ok", "end:Delegation facts Ok" }));
    EXPECT_NE(m_started.at(0).id, m_started.at(1).id);
    EXPECT_EQ(m_started.at(2).parentId, m_started.at(1).id);
    EXPECT_EQ(delegateClient(1).held.size(), 0u);
    EXPECT_EQ(mainClient().held.size(), 1u);
    EXPECT_EQ(m_clients.at(2)->requested.keep, false);
}

TEST_F(ChatAgentTest, Delegation_ThatFails_ReachesTheMainAgentAsItsResult)
{
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    delegateClient().completeNext(failedCompletion(QStringLiteral("server_error")));

    EXPECT_TRUE(lastMessageOf(mainClient())
                    .contains(QStringLiteral("The Courses agent failed (server_error).")));
    EXPECT_EQ(m_events,
              (QStringList { "start:Delegation courses", "end:Delegation courses Failed" }));
    mainClient().completeNext(contentCompletion(kAnswer));
    EXPECT_EQ(m_events.last(), "finished:" + kAnswer);
}

TEST_F(ChatAgentTest, Delegation_ThatFails_KeepsTheFailureReportAsTheStepOutput)
{
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    delegateClient().completeNext(failedCompletion(QStringLiteral("server_error")));

    ASSERT_EQ(m_ended.size(), 1u);
    EXPECT_EQ(m_ended.at(0).status, StepStatus::Failed);
    EXPECT_EQ(m_ended.at(0).output.toString(),
              QStringLiteral("The Courses agent failed (server_error)."));
}

TEST_F(ChatAgentTest, Delegation_ThatHitsTheToolCallLimit_EndsAsAFailedStepAndTellsTheMainAgent)
{
    ::Agent agent(QStringLiteral("model"), kCoursesPrompt);
    agent.setMaxToolCalls(2);
    agent.addTool(kListCourses, appTool());
    m_catalog = AgentCatalog();
    m_catalog.add(AppAgent { QStringLiteral("courses"), QStringLiteral("Courses"),
                             kCoursesDescription, agent });
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    for (int round = 0; round < 3; ++round)
    {
        delegateClient().completeNext(toolCallCompletion(
            QStringLiteral("t%1").arg(round), QStringLiteral("list_courses"),
            { { QStringLiteral("limit"), 3 } }));
    }

    EXPECT_EQ(m_toolArguments.size(), 2u);
    EXPECT_TRUE(lastMessageOf(mainClient())
                    .contains(QStringLiteral("The Courses agent failed (too_many_tool_calls).")));
    ASSERT_FALSE(m_ended.empty());
    EXPECT_EQ(m_ended.back().kind, StepKind::Delegation);
    EXPECT_EQ(m_ended.back().status, StepStatus::Failed);
}

TEST_F(ChatAgentTest, Delegation_TheCeilingDefaultsToThree)
{
    EXPECT_EQ(AppAgent::kDefaultMaxDelegations, 3);
    EXPECT_EQ(coursesAgent().maxDelegations, AppAgent::kDefaultMaxDelegations);
}

TEST_F(ChatAgentTest, Delegation_PastTheCeiling_FailsWithTheLimitTokenAndTellsTheMainAgent)
{
    limitCoursesAgentTo(2);
    beginAndAsk();
    delegateAndReport(QStringLiteral("courses"), 0);
    delegateAndReport(QStringLiteral("courses"), 1);
    const size_t clients = m_clients.size();

    callDelegate(QStringLiteral("courses"));

    EXPECT_EQ(m_clients.size(), clients);
    EXPECT_TRUE(lastMessageOf(mainClient())
                    .contains(QStringLiteral("The Courses agent failed (too_many_delegations).")));
    EXPECT_EQ(m_events.last(), "end:Delegation courses Failed");
    ASSERT_FALSE(m_ended.empty());
    EXPECT_EQ(m_ended.back().status, StepStatus::Failed);
    EXPECT_EQ(m_ended.back().output.toString(),
              QStringLiteral("The Courses agent failed (too_many_delegations)."));
}

TEST_F(ChatAgentTest, Delegation_PastTheCeiling_DoesNotAskTheAgentForAFollowUp)
{
    limitCoursesAgentTo(1);
    beginAndAsk();
    delegateAndReport(QStringLiteral("courses"), 0);
    const int calls = m_followUpCalls;

    callDelegate(QStringLiteral("courses"));

    EXPECT_EQ(m_followUpCalls, calls);
}

TEST_F(ChatAgentTest, Delegation_WithinTheCeiling_StillRunsTheAgent)
{
    limitCoursesAgentTo(2);
    beginAndAsk();
    delegateAndReport(QStringLiteral("courses"), 0);
    delegateAndReport(QStringLiteral("courses"), 1);

    EXPECT_EQ(m_clients.size(), 3u);
    EXPECT_EQ(m_events.count("end:Delegation courses Ok"), 2);
    EXPECT_EQ(m_events.count("end:Delegation courses Failed"), 0);
}

TEST_F(ChatAgentTest, Delegation_TheCeilingCountsPerAgent)
{
    limitCoursesAgentTo(1);
    beginAndAsk();
    delegateAndReport(QStringLiteral("courses"), 0);

    delegateAndReport(QStringLiteral("facts"), 1);

    EXPECT_EQ(m_events.last(), "end:Delegation facts Ok");
}

TEST_F(ChatAgentTest, Delegation_TheCeilingStartsAgainWithTheNextTurn)
{
    limitCoursesAgentTo(1);
    beginAndAsk();
    delegateAndReport(QStringLiteral("courses"), 0);
    m_turn.reset();
    m_clients.clear();
    m_events.clear();
    beginAndAsk();

    delegateAndReport(QStringLiteral("courses"), 0);

    EXPECT_EQ(m_events.last(), "end:Delegation courses Ok");
}

TEST_F(ChatAgentTest, Delegation_AfterAFailedOne_ASuccessfulOneEndsOk)
{
    beginAndAsk();
    mainClient().completeNext(
        toolCallsCompletion({ toolCall(QStringLiteral("c1"), QStringLiteral("courses"),
                                       { { QStringLiteral("task"), QStringLiteral("a") } }),
                              toolCall(QStringLiteral("c2"), QStringLiteral("facts"),
                                       { { QStringLiteral("task"), QStringLiteral("b") } }) }));
    delegateClient(0).completeNext(failedCompletion(QStringLiteral("server_error")));
    delegateClient(1).completeNext(contentCompletion(QStringLiteral("B done")));

    EXPECT_EQ(m_events,
              (QStringList { "start:Delegation courses", "end:Delegation courses Failed",
                             "start:Delegation facts", "end:Delegation facts Ok" }));
}

TEST_F(ChatAgentTest, Delegation_ThatFailsBeforeReturningFromTheRequest_StillReachesTheMainAgent)
{
    m_delegateImmediate = failedCompletion(QStringLiteral("network_error"));
    beginAndAsk();

    callDelegate(QStringLiteral("courses"));

    EXPECT_TRUE(lastMessageOf(mainClient())
                    .contains(QStringLiteral("The Courses agent failed (network_error).")));
    EXPECT_EQ(m_events,
              (QStringList { "start:Delegation courses", "end:Delegation courses Failed" }));
}

TEST_F(ChatAgentTest, Delegation_WhenTheFactoryGivesNoClient_ReportsItAsUnavailable)
{
    m_refuseDelegates = true;
    beginAndAsk();

    callDelegate(QStringLiteral("courses"));

    EXPECT_TRUE(lastMessageOf(mainClient())
                    .contains(QStringLiteral("The Courses agent failed (unavailable).")));
    EXPECT_EQ(m_events,
              (QStringList { "start:Delegation courses", "end:Delegation courses Failed" }));
}

TEST_F(ChatAgentTest, Cancel_WhileTheMainAgentWaits_FailsWithCancelledOnce)
{
    beginAndAsk();

    m_turn->cancel();
    mainClient().completeNext(contentCompletion(QStringLiteral("late")));

    EXPECT_EQ(m_events, QStringList { QStringLiteral("failed:cancelled") });
}

TEST_F(ChatAgentTest, Cancel_BeforeTheQuestionWasSent_NeverSendsIt)
{
    begin(request());

    m_turn->cancel();
    settle();

    EXPECT_TRUE(mainClient().held.empty());
    EXPECT_EQ(m_events, QStringList { QStringLiteral("failed:cancelled") });
}

TEST_F(ChatAgentTest, Cancel_DuringADelegation_ClosesTheOpenStepsAndIgnoresLateResults)
{
    m_holdTools = true;
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));
    delegateClient().completeNext(toolCallCompletion(
        QStringLiteral("t1"), QStringLiteral("list_courses"), { { QStringLiteral("limit"), 3 } }));
    ASSERT_EQ(m_heldTools.size(), 1u);

    m_turn->cancel();
    m_heldTools.front()(kToolResult);

    EXPECT_EQ(m_events,
              (QStringList { "start:Delegation courses", "start:Tool courses list_courses",
                             "end:Tool courses list_courses Failed",
                             "end:Delegation courses Failed", "failed:cancelled" }));
    EXPECT_TRUE(delegateClient().held.empty());
    EXPECT_TRUE(mainClient().held.empty());
}

TEST_F(ChatAgentTest, Cancel_WhileADelegateWaitsForItsModel_IgnoresItsLateAnswer)
{
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    m_turn->cancel();
    delegateClient().completeNext(contentCompletion(kReport));

    EXPECT_EQ(m_events,
              (QStringList { "start:Delegation courses", "end:Delegation courses Failed",
                             "failed:cancelled" }));
    EXPECT_TRUE(mainClient().held.empty());
}

TEST_F(ChatAgentTest, Cancel_Twice_FailsOnce)
{
    beginAndAsk();

    m_turn->cancel();
    m_turn->cancel();

    EXPECT_EQ(m_events, QStringList { QStringLiteral("failed:cancelled") });
}

TEST_F(ChatAgentTest, Cancel_AfterTheTurnFinished_ChangesNothing)
{
    beginAndAsk();
    mainClient().completeNext(contentCompletion(kAnswer));
    const QStringList before = m_events;

    m_turn->cancel();

    EXPECT_EQ(m_events, before);
}

TEST_F(ChatAgentTest, Cancel_ThenDestroyingTheTurn_ReleasesEveryClient)
{
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    m_turn->cancel();
    m_turn.reset();

    ASSERT_EQ(m_clients.size(), 2u);
    EXPECT_TRUE(m_clients.at(0)->destroyed);
    EXPECT_TRUE(m_clients.at(1)->destroyed);
}

TEST_F(ChatAgentTest, Failed_AfterTheMainAgentFailed_IgnoresAnyLaterOutcome)
{
    beginAndAsk();
    mainClient().completeNext(failedCompletion(QStringLiteral("server_error")));

    m_turn->cancel();

    EXPECT_EQ(m_events, QStringList { QStringLiteral("failed:server_error") });
}

TEST_F(ChatAgentTest, FollowUp_ANonEmptyResult_GoesToTheSameDelegateOnAFreshClient)
{
    m_followUps = { QStringLiteral("check again") };
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    delegateClient(0).completeNext(contentCompletion(QStringLiteral("first report")));

    ASSERT_EQ(m_clients.size(), 3u);
    EXPECT_TRUE(mainClient().held.empty());
    EXPECT_TRUE(delegateClient(0).held.empty());
    EXPECT_FALSE(delegateClient(1).requested.keep);
    ASSERT_EQ(delegateClient(1).held.size(), 1u);
    const ::Messages& sent = delegateClient(1).held.front().messages;
    ASSERT_EQ(sent.size(), 4);
    EXPECT_EQ(sent.at(0).role, QStringLiteral("system"));
    EXPECT_EQ(sent.at(0).content, kCoursesPrompt);
    EXPECT_EQ(sent.at(1).content, kTask);
    EXPECT_EQ(sent.at(2).role, QStringLiteral("assistant"));
    EXPECT_EQ(sent.at(2).content, QStringLiteral("first report"));
    EXPECT_EQ(sent.at(3).role, QStringLiteral("user"));
    EXPECT_EQ(sent.at(3).content, QStringLiteral("check again"));
}

TEST_F(ChatAgentTest, FollowUp_TheSecondOne_CarriesBothEarlierRoundsToItsOwnClient)
{
    m_followUps = { QStringLiteral("f1"), QStringLiteral("f2") };
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));
    delegateClient(0).completeNext(contentCompletion(QStringLiteral("r0")));

    delegateClient(1).completeNext(contentCompletion(QStringLiteral("r1")));

    ASSERT_EQ(m_clients.size(), 4u);
    ASSERT_EQ(delegateClient(2).held.size(), 1u);
    const ::Messages& sent = delegateClient(2).held.front().messages;
    ASSERT_EQ(sent.size(), 6);
    EXPECT_EQ(sent.at(1).content, kTask);
    EXPECT_EQ(sent.at(2).content, QStringLiteral("r0"));
    EXPECT_EQ(sent.at(3).content, QStringLiteral("f1"));
    EXPECT_EQ(sent.at(4).content, QStringLiteral("r1"));
    EXPECT_EQ(sent.at(5).content, QStringLiteral("f2"));
}

TEST_F(ChatAgentTest, FollowUp_WhenNoClientCanBeCreated_TheReportAlreadyGivenIsDelivered)
{
    m_followUps = { QStringLiteral("check again") };
    m_clientLimit = 2;
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    delegateClient(0).completeNext(contentCompletion(QStringLiteral("first report")));

    EXPECT_EQ(m_clients.size(), 2u);
    ASSERT_EQ(mainClient().held.size(), 1u);
    EXPECT_TRUE(lastMessageOf(mainClient()).contains(QStringLiteral("first report")));
    EXPECT_EQ(m_followUpCalls, 1);
}

TEST_F(ChatAgentTest, FollowUp_AfterTheDelegateAnswersIt_TheLastReportGoesToTheMainAgent)
{
    m_followUps = { QStringLiteral("check again") };
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));
    delegateClient(0).completeNext(contentCompletion(QStringLiteral("first report")));

    delegateClient(1).completeNext(contentCompletion(kReport));

    ASSERT_EQ(mainClient().held.size(), 1u);
    EXPECT_TRUE(lastMessageOf(mainClient()).contains(kReport));
    EXPECT_FALSE(lastMessageOf(mainClient()).contains(QStringLiteral("first report")));
    EXPECT_EQ(m_followUpCalls, 2);
}

TEST_F(ChatAgentTest, FollowUp_AnEmptyResult_ReportsAsBefore)
{
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    delegateClient().completeNext(contentCompletion(kReport));

    EXPECT_TRUE(delegateClient().held.empty());
    ASSERT_EQ(mainClient().held.size(), 1u);
    EXPECT_TRUE(lastMessageOf(mainClient()).contains(kReport));
    EXPECT_EQ(m_followUpCalls, 1);
}

TEST_F(ChatAgentTest, FollowUp_AnAgentWithoutOne_ReportsAsBefore)
{
    beginAndAsk();
    callDelegate(QStringLiteral("facts"));

    delegateClient().completeNext(contentCompletion(kReport));

    EXPECT_TRUE(delegateClient().held.empty());
    ASSERT_EQ(mainClient().held.size(), 1u);
    EXPECT_TRUE(lastMessageOf(mainClient()).contains(kReport));
    EXPECT_EQ(m_followUpCalls, 0);
}

TEST_F(ChatAgentTest, FollowUp_IsBoundedAtTwoPerDelegation)
{
    m_followUps = { QStringLiteral("f1"), QStringLiteral("f2"), QStringLiteral("f3") };
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    delegateClient(0).completeNext(contentCompletion(QStringLiteral("r0")));
    EXPECT_EQ(lastMessageOf(delegateClient(1)), QStringLiteral("f1"));
    delegateClient(1).completeNext(contentCompletion(QStringLiteral("r1")));
    EXPECT_EQ(lastMessageOf(delegateClient(2)), QStringLiteral("f2"));
    delegateClient(2).completeNext(contentCompletion(QStringLiteral("r2")));

    EXPECT_EQ(m_clients.size(), 4u);
    EXPECT_TRUE(delegateClient(2).held.empty());
    ASSERT_EQ(mainClient().held.size(), 1u);
    EXPECT_TRUE(lastMessageOf(mainClient()).contains(QStringLiteral("r2")));
    EXPECT_EQ(m_followUpCalls, 2);
    EXPECT_EQ(m_followUps, QStringList { QStringLiteral("f3") });
}

TEST_F(ChatAgentTest, FollowUp_TheBoundCountsPerDelegationNotPerTurn)
{
    m_followUps = { QStringLiteral("f1"), QStringLiteral("f2"), QStringLiteral("f3") };
    beginAndAsk();
    mainClient().completeNext(
        toolCallsCompletion({ toolCall(QStringLiteral("c1"), QStringLiteral("courses"),
                                       { { QStringLiteral("task"), QStringLiteral("a") } }),
                              toolCall(QStringLiteral("c2"), QStringLiteral("courses"),
                                       { { QStringLiteral("task"), QStringLiteral("b") } }) }));
    delegateClient(0).completeNext(contentCompletion(QStringLiteral("a0")));
    delegateClient(1).completeNext(contentCompletion(QStringLiteral("a1")));
    delegateClient(2).completeNext(contentCompletion(QStringLiteral("a2")));
    ASSERT_EQ(m_clients.size(), 5u);

    delegateClient(3).completeNext(contentCompletion(QStringLiteral("b0")));

    EXPECT_EQ(lastMessageOf(delegateClient(4)), QStringLiteral("f3"));
}

TEST_F(ChatAgentTest, FollowUp_ToolStepsOfTheFollowUp_StayUnderTheSameDelegation)
{
    m_followUps = { QStringLiteral("check again") };
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));
    delegateClient(0).completeNext(contentCompletion(QStringLiteral("first report")));

    delegateClient(1).completeNext(toolCallCompletion(
        QStringLiteral("t1"), QStringLiteral("list_courses"), { { QStringLiteral("limit"), 3 } }));
    delegateClient(1).completeNext(contentCompletion(kReport));

    EXPECT_EQ(m_events,
              (QStringList { "start:Delegation courses", "start:Tool courses list_courses",
                             "end:Tool courses list_courses Ok", "end:Delegation courses Ok" }));
    ASSERT_EQ(m_started.size(), 2u);
    EXPECT_EQ(m_started.at(1).parentId, m_started.at(0).id);
}

TEST_F(ChatAgentTest, FollowUp_ADelegateThatFailsOnIt_ReachesTheMainAgentAsItsResult)
{
    m_followUps = { QStringLiteral("check again") };
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));
    delegateClient(0).completeNext(contentCompletion(QStringLiteral("first report")));

    delegateClient(1).completeNext(failedCompletion(QStringLiteral("server_error")));

    ASSERT_EQ(mainClient().held.size(), 1u);
    EXPECT_TRUE(lastMessageOf(mainClient()).contains(QStringLiteral("server_error")));
    EXPECT_EQ(m_followUpCalls, 1);
}

TEST_F(ChatAgentTest, FollowUp_CancelWhileItRuns_ClosesTheStepsAndIgnoresTheLateAnswer)
{
    m_followUps = { QStringLiteral("check again") };
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));
    delegateClient(0).completeNext(contentCompletion(QStringLiteral("first report")));
    ASSERT_EQ(delegateClient(1).held.size(), 1u);

    m_turn->cancel();
    delegateClient(1).completeNext(contentCompletion(kReport));

    EXPECT_EQ(m_events,
              (QStringList { "start:Delegation courses", "end:Delegation courses Failed",
                             "failed:cancelled" }));
    EXPECT_TRUE(mainClient().held.empty());
    EXPECT_EQ(m_followUpCalls, 1);
}

TEST_F(ChatAgentTest, Context_IsAppendedToTheQuestionOfTheMainAgent)
{
    begin(requestWithContext(stopwatchContext()));
    settle();

    EXPECT_EQ(lastMessageOf(mainClient()), kQuestion + kContextLine);
}

TEST_F(ChatAgentTest, Context_IsAppendedToEveryDelegationTask)
{
    begin(requestWithContext(stopwatchContext()));
    settle();

    callDelegate(QStringLiteral("courses"));

    EXPECT_EQ(lastMessageOf(delegateClient()), kTask + kContextLine);
}

TEST_F(ChatAgentTest, Context_WithoutData_StillNamesTheTitle)
{
    chat::ChatContext bare = stopwatchContext();
    bare.data = QJsonObject();
    begin(requestWithContext(bare));
    settle();

    EXPECT_EQ(lastMessageOf(mainClient()), kQuestion + QStringLiteral("\n[context: Stopwatch {}]"));
}

TEST_F(ChatAgentTest, Context_Absent_LeavesTheQuestionAndTheTaskUnchanged)
{
    beginAndAsk();
    EXPECT_EQ(lastMessageOf(mainClient()), kQuestion);

    callDelegate(QStringLiteral("courses"));

    EXPECT_EQ(lastMessageOf(delegateClient()), kTask);
}

TEST_F(ChatAgentTest, Delegate_RunsOnTheModelAndReasoningEffortOfTheTurn)
{
    TurnRequest chosen = request();
    chosen.model = QStringLiteral("chosen-model");
    chosen.reasoningEffort = QStringLiteral("high");
    begin(chosen);
    settle();

    callDelegate(QStringLiteral("courses"));

    const ModelConfig& config = delegateClient().held.front().config;
    EXPECT_EQ(config.model, QStringLiteral("chosen-model"));
    EXPECT_EQ(config.reasoningEffort, QStringLiteral("high"));
}

TEST_F(ChatAgentTest, Delegate_WithItsOwnModelAndReasoningEffort_RunsOnThemInsteadOfTheTurn)
{
    useCoursesAgent(QStringLiteral("own-model"), QStringLiteral("max"));
    TurnRequest chosen = request();
    chosen.model = QStringLiteral("chosen-model");
    chosen.reasoningEffort = QStringLiteral("high");
    begin(chosen);
    settle();
    ASSERT_EQ(mainClient().held.size(), 1u);
    const ModelConfig mainConfig = mainClient().held.front().config;

    callDelegate(QStringLiteral("courses"));

    const ModelConfig& config = delegateClient().held.front().config;
    EXPECT_EQ(config.model, QStringLiteral("own-model"));
    EXPECT_EQ(config.reasoningEffort, QStringLiteral("max"));
    EXPECT_EQ(mainConfig.model, QStringLiteral("chosen-model"));
    EXPECT_EQ(mainConfig.reasoningEffort, QStringLiteral("high"));
}

TEST_F(ChatAgentTest, Delegate_WithOnlyItsOwnModel_FollowsTheReasoningEffortOfTheTurn)
{
    useCoursesAgent(QStringLiteral("own-model"), QString());
    TurnRequest chosen = request();
    chosen.model = QStringLiteral("chosen-model");
    chosen.reasoningEffort = QStringLiteral("high");
    begin(chosen);
    settle();

    callDelegate(QStringLiteral("courses"));

    const ModelConfig& config = delegateClient().held.front().config;
    EXPECT_EQ(config.model, QStringLiteral("own-model"));
    EXPECT_EQ(config.reasoningEffort, QStringLiteral("high"));
}

TEST_F(ChatAgentTest, Delegate_WithOnlyItsOwnReasoningEffort_FollowsTheModelOfTheTurn)
{
    useCoursesAgent(QString(), QStringLiteral("max"));
    TurnRequest chosen = request();
    chosen.model = QStringLiteral("chosen-model");
    chosen.reasoningEffort = QStringLiteral("high");
    begin(chosen);
    settle();

    callDelegate(QStringLiteral("courses"));

    const ModelConfig& config = delegateClient().held.front().config;
    EXPECT_EQ(config.model, QStringLiteral("chosen-model"));
    EXPECT_EQ(config.reasoningEffort, QStringLiteral("max"));
}

TEST_F(ChatAgentTest, Delegate_WithAChosenModel_RunsOnItInsteadOfItsOwnModelAndTheTurn)
{
    useCoursesAgent(QStringLiteral("own-model"), QStringLiteral("max"));
    TurnRequest chosen = request();
    chosen.model = QStringLiteral("chosen-model");
    chosen.agentModels.insert(QStringLiteral("courses"), QStringLiteral("picked-model"));
    begin(chosen);
    settle();

    callDelegate(QStringLiteral("courses"));

    const ModelConfig& config = delegateClient().held.front().config;
    EXPECT_EQ(config.model, QStringLiteral("picked-model"));
    EXPECT_EQ(config.reasoningEffort, QStringLiteral("max"));
}

TEST_F(ChatAgentTest, Delegate_WithAChosenModelOfAnotherAgent_KeepsItsOwnModel)
{
    useCoursesAgent(QStringLiteral("own-model"), QString());
    TurnRequest chosen = request();
    chosen.agentModels.insert(QStringLiteral("facts"), QStringLiteral("picked-model"));
    begin(chosen);
    settle();

    callDelegate(QStringLiteral("courses"));

    EXPECT_EQ(delegateClient().held.front().config.model, QStringLiteral("own-model"));
}

TEST_F(ChatAgentTest, FollowUp_KeepsTheChosenModelOfTheDelegate)
{
    useCoursesAgent(QStringLiteral("own-model"), QStringLiteral("max"));
    m_followUps = { QStringLiteral("check again") };
    TurnRequest chosen = request();
    chosen.agentModels.insert(QStringLiteral("courses"), QStringLiteral("picked-model"));
    begin(chosen);
    settle();
    callDelegate(QStringLiteral("courses"));

    delegateClient(0).completeNext(contentCompletion(QStringLiteral("first report")));

    ASSERT_EQ(delegateClient(1).held.size(), 1u);
    EXPECT_EQ(delegateClient(1).held.front().config.model, QStringLiteral("picked-model"));
}

TEST_F(ChatAgentTest, FollowUp_KeepsTheOwnModelAndReasoningEffortOfTheDelegate)
{
    useCoursesAgent(QStringLiteral("own-model"), QStringLiteral("max"));
    m_followUps = { QStringLiteral("check again") };
    beginAndAsk();
    callDelegate(QStringLiteral("courses"));

    delegateClient(0).completeNext(contentCompletion(QStringLiteral("first report")));

    ASSERT_EQ(delegateClient(1).held.size(), 1u);
    const ModelConfig& config = delegateClient(1).held.front().config;
    EXPECT_EQ(config.model, QStringLiteral("own-model"));
    EXPECT_EQ(config.reasoningEffort, QStringLiteral("max"));
}

namespace
{
const QString kSecondTask = QStringLiteral("change the title");
const QString kThirdTask = QStringLiteral("add a button");
const QString kFirstReport = QStringLiteral("Built it.");
const QString kSecondReport = QStringLiteral("Changed it.");
const QString kFirstSession = QStringLiteral("delegate-session-1");
const QString kSecondSession = QStringLiteral("delegate-session-2");
constexpr int kOverTheLimit = kMaxInputTokens + 1;
}

TEST_F(ChatAgentTest, KeptContext_TheFirstDelegation_AsksForAKeptSessionAndStartsFromTheTask)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();

    delegateTo(mainIndex);

    ASSERT_EQ(m_clients.size(), 2u);
    EXPECT_TRUE(m_clients.at(1)->requested.keep);
    EXPECT_TRUE(m_clients.at(1)->requested.resume.isEmpty());
    EXPECT_EQ(sentContents(1), (QStringList { kCoursesPrompt, kTask }));
}

TEST_F(ChatAgentTest, KeptContext_ASecondDelegationOfTheSameTurn_ContinuesTheFirstOne)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(1000, kFirstReport));

    delegateTo(mainIndex, kSecondTask);

    ASSERT_EQ(m_clients.size(), 3u);
    EXPECT_EQ(m_clients.at(2)->requested.resume, kFirstSession);
    EXPECT_TRUE(m_clients.at(2)->requested.keep);
    EXPECT_EQ(sentContents(2), (QStringList { kCoursesPrompt, kTask, kFirstReport, kSecondTask }));
    EXPECT_TRUE(m_forgotten.isEmpty());
}

TEST_F(ChatAgentTest, KeptContext_ADelegationOfTheNextTurn_ContinuesTheEarlierOne)
{
    keepCoursesContext();
    const size_t firstTurn = beginTurn();
    runDelegation(firstTurn, kTask, usingTokens(1000, kFirstReport));
    answer(firstTurn);

    const size_t secondTurn = beginTurn();
    delegateTo(secondTurn, kSecondTask);

    ASSERT_EQ(m_clients.size(), 4u);
    EXPECT_EQ(m_clients.at(3)->requested.resume, kFirstSession);
    EXPECT_EQ(sentContents(3), (QStringList { kCoursesPrompt, kTask, kFirstReport, kSecondTask }));
}

TEST_F(ChatAgentTest, KeptContext_TheDelegateKeepsGrowingAcrossThreeDelegations)
{
    keepCoursesContext();
    const size_t firstTurn = beginTurn();
    runDelegation(firstTurn, kTask, usingTokens(1000, kFirstReport));
    runDelegation(firstTurn, kSecondTask, usingTokens(2000, kSecondReport));

    delegateTo(firstTurn, kThirdTask);

    EXPECT_EQ(sentContents(3),
              (QStringList { kCoursesPrompt, kTask, kFirstReport, kSecondTask, kSecondReport,
                             kThirdTask }));
}

TEST_F(ChatAgentTest, KeptContext_TheCurrentModelIsUsedOnTheContinuedDelegation)
{
    keepCoursesContext();
    const size_t firstTurn = beginTurn();
    runDelegation(firstTurn, kTask, usingTokens(1000, kFirstReport));
    answer(firstTurn);
    TurnRequest chosen = request();
    chosen.agentModels.insert(QStringLiteral("courses"), QStringLiteral("picked-model"));

    const size_t secondTurn = beginTurn(chosen);
    delegateTo(secondTurn, kSecondTask);

    ASSERT_EQ(m_clients.at(3)->held.size(), 1u);
    EXPECT_EQ(m_clients.at(3)->held.front().config.model, QStringLiteral("picked-model"));
}

TEST_F(ChatAgentTest, KeptContext_AnAgentWithoutThePolicy_StartsFreshEveryTime)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    m_clients.at(mainIndex)->completeNext(
        toolCallCompletion(QStringLiteral("call-facts"), QStringLiteral("facts"),
                           { { QStringLiteral("task"), kTask } }));
    report(usingTokens(kOverTheLimit, kFirstReport));

    m_clients.at(mainIndex)->completeNext(
        toolCallCompletion(QStringLiteral("call-facts-2"), QStringLiteral("facts"),
                           { { QStringLiteral("task"), kSecondTask } }));

    ASSERT_EQ(m_clients.size(), 3u);
    EXPECT_FALSE(m_clients.at(2)->requested.keep);
    EXPECT_TRUE(m_clients.at(2)->requested.resume.isEmpty());
    EXPECT_EQ(sentContents(2), (QStringList { kFactsPrompt, kSecondTask }));
    EXPECT_TRUE(m_forgotten.isEmpty());
}

TEST_F(ChatAgentTest, KeptContext_AnotherConversation_DoesNotShareIt)
{
    keepCoursesContext();
    const size_t firstTurn = beginTurn();
    runDelegation(firstTurn, kTask, usingTokens(1000, kFirstReport));
    answer(firstTurn);
    m_conversation = QUuid::createUuid();

    const size_t otherTurn = beginTurn();
    delegateTo(otherTurn, kSecondTask);

    EXPECT_TRUE(m_clients.at(3)->requested.resume.isEmpty());
    EXPECT_EQ(sentContents(3), (QStringList { kCoursesPrompt, kSecondTask }));
    EXPECT_TRUE(m_forgotten.isEmpty());
}

TEST_F(ChatAgentTest, KeptContext_AtExactlyTheMaximumAge_StillContinues)
{
    keepCoursesContext();
    const QDateTime start = m_now;
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    m_now = start.addSecs(55 * 60);

    delegateTo(mainIndex, kSecondTask);

    EXPECT_EQ(m_clients.at(2)->requested.resume, kFirstSession);
    EXPECT_EQ(sentContents(2).constLast(), kCompactionRequest);
}

TEST_F(ChatAgentTest, KeptContext_OneMillisecondPastTheMaximumAge_ClearsWithoutCompacting)
{
    keepCoursesContext();
    const QDateTime start = m_now;
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    m_now = start.addSecs(55 * 60).addMSecs(1);

    delegateTo(mainIndex, kSecondTask);

    ASSERT_EQ(m_clients.size(), 3u);
    EXPECT_TRUE(m_clients.at(2)->requested.resume.isEmpty());
    EXPECT_TRUE(m_clients.at(2)->requested.keep);
    EXPECT_EQ(sentContents(2), (QStringList { kCoursesPrompt, kSecondTask }));
    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
}

TEST_F(ChatAgentTest, KeptContext_TheAgeCountsFromTheStartOfTheLastRequest)
{
    keepCoursesContext();
    const QDateTime start = m_now;
    const size_t mainIndex = beginTurn();
    delegateTo(mainIndex);
    m_now = start.addSecs(40 * 60);
    report(usingTokens(1000, kFirstReport));
    m_now = start.addSecs(56 * 60);

    delegateTo(mainIndex, kSecondTask);

    EXPECT_TRUE(m_clients.at(2)->requested.resume.isEmpty());
    EXPECT_EQ(sentContents(2), (QStringList { kCoursesPrompt, kSecondTask }));
    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
}

TEST_F(ChatAgentTest, KeptContext_AFollowUpRequestRenewsTheStartOfTheLastRequest)
{
    keepCoursesContext();
    m_followUps = { QStringLiteral("check again") };
    const QDateTime start = m_now;
    const size_t mainIndex = beginTurn();
    delegateTo(mainIndex);
    m_now = start.addSecs(10 * 60);
    report(contentCompletion(QStringLiteral("first report")));
    report(usingTokens(1000, kFirstReport));
    m_now = start.addSecs(65 * 60);

    delegateTo(mainIndex, kSecondTask);

    EXPECT_EQ(m_clients.at(3)->requested.resume, kFirstSession);
    EXPECT_EQ(sentContents(3),
              (QStringList { kCoursesPrompt, kTask, QStringLiteral("first report"),
                             QStringLiteral("check again"), kFirstReport, kSecondTask }));
}

TEST_F(ChatAgentTest, KeptContext_AFollowUp_ResumesTheSessionOfTheDelegation)
{
    keepCoursesContext();
    m_followUps = { QStringLiteral("check again") };
    const size_t mainIndex = beginTurn();
    delegateTo(mainIndex);

    report(contentCompletion(QStringLiteral("first report")));

    ASSERT_EQ(m_clients.size(), 3u);
    EXPECT_EQ(m_clients.at(2)->requested.resume, kFirstSession);
    EXPECT_TRUE(m_clients.at(2)->requested.keep);
}

TEST_F(ChatAgentTest, KeptContext_TheInputTokensOfTheLastRequestDecide_NotTheEarlierOnes)
{
    keepCoursesContext();
    m_followUps = { QStringLiteral("check again") };
    const size_t mainIndex = beginTurn();
    delegateTo(mainIndex);
    report(usingTokens(kOverTheLimit, QStringLiteral("first report")));
    report(usingTokens(1000, kFirstReport));

    delegateTo(mainIndex, kSecondTask);

    EXPECT_EQ(sentContents(3).constLast(), kSecondTask);
}

TEST_F(ChatAgentTest, KeptContext_ExactlyTheMaximumInputTokens_StillContinues)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kMaxInputTokens, kFirstReport));

    delegateTo(mainIndex, kSecondTask);

    EXPECT_EQ(sentContents(2), (QStringList { kCoursesPrompt, kTask, kFirstReport, kSecondTask }));
}

TEST_F(ChatAgentTest, KeptContext_OneTokenOverTheMaximum_SendsTheCompactionRequestFirst)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));

    delegateTo(mainIndex, kSecondTask);

    ASSERT_EQ(m_clients.size(), 3u);
    EXPECT_EQ(m_clients.at(2)->requested.resume, kFirstSession);
    EXPECT_TRUE(m_clients.at(2)->requested.keep);
    EXPECT_EQ(sentContents(2),
              (QStringList { kCoursesPrompt, kTask, kFirstReport, kCompactionRequest }));
    EXPECT_EQ(sentTo(2).constLast().role, QStringLiteral("user"));
    EXPECT_TRUE(m_forgotten.isEmpty());
}

TEST_F(ChatAgentTest, KeptContext_TheCompactionRequestKeepsTheToolsOfTheDelegate)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));

    delegateTo(mainIndex, kSecondTask);

    EXPECT_TRUE(m_clients.at(2)->held.front().tools.contains(QStringLiteral("list_courses")));
}

TEST_F(ChatAgentTest, KeptContext_AfterTheSummary_TheTaskGoesToAFreshContextHoldingOnlyIt)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    delegateTo(mainIndex, kSecondTask);

    report(contentCompletion(kSummary));

    ASSERT_EQ(m_clients.size(), 4u);
    EXPECT_TRUE(m_clients.at(3)->requested.resume.isEmpty());
    EXPECT_TRUE(m_clients.at(3)->requested.keep);
    EXPECT_EQ(sentContents(3), (QStringList { kCoursesPrompt, kSummaryMessage, kSecondTask }));
    EXPECT_EQ(sentTo(3).at(1).role, QStringLiteral("user"));
    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
}

TEST_F(ChatAgentTest, KeptContext_TheCompactedDelegation_ReportsToTheMainAgentAsOneDelegation)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    delegateTo(mainIndex, kSecondTask);
    report(contentCompletion(kSummary));

    report(usingTokens(5000, kSecondReport));

    EXPECT_TRUE(lastMessageOf(*m_clients.at(mainIndex)).contains(kSecondReport));
    EXPECT_EQ(m_events.count("start:Delegation courses"), 2);
    EXPECT_EQ(m_events.count("end:Delegation courses Ok"), 2);
    EXPECT_EQ(m_events.count("end:Delegation courses Failed"), 0);
}

TEST_F(ChatAgentTest, KeptContext_AfterACompaction_TheNextDelegationContinuesTheNewContext)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    delegateTo(mainIndex, kSecondTask);
    report(contentCompletion(kSummary));
    report(usingTokens(5000, kSecondReport));

    delegateTo(mainIndex, kThirdTask);

    ASSERT_EQ(m_clients.size(), 5u);
    EXPECT_EQ(m_clients.at(4)->requested.resume, kSecondSession);
    EXPECT_EQ(
        sentContents(4),
        (QStringList { kCoursesPrompt, kSummaryMessage, kSecondTask, kSecondReport, kThirdTask }));
}

TEST_F(ChatAgentTest, KeptContext_ACompactionDoesNotUseUpTheDelegationCeiling)
{
    keepCoursesContext(keepPolicy(), 2);
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    delegateTo(mainIndex, kSecondTask);
    report(contentCompletion(kSummary));
    report(usingTokens(5000, kSecondReport));
    EXPECT_EQ(m_events.count("end:Delegation courses Ok"), 2);

    delegateTo(mainIndex, kThirdTask);

    EXPECT_TRUE(lastMessageOf(*m_clients.at(mainIndex))
                    .contains(QStringLiteral("The Courses agent failed (too_many_delegations).")));
}

TEST_F(ChatAgentTest, KeptContext_AFailedCompaction_FallsBackToAClearedContext)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    delegateTo(mainIndex, kSecondTask);

    report(failedCompletion(QStringLiteral("server_error")));

    ASSERT_EQ(m_clients.size(), 4u);
    EXPECT_TRUE(m_clients.at(3)->requested.resume.isEmpty());
    EXPECT_EQ(sentContents(3), (QStringList { kCoursesPrompt, kSecondTask }));
    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
    report(usingTokens(5000, kSecondReport));
    EXPECT_EQ(m_events.count("end:Delegation courses Ok"), 2);
    EXPECT_FALSE(m_events.contains(QStringLiteral("failed:server_error")));
}

TEST_F(ChatAgentTest, KeptContext_ASummaryTheValidatorRejects_FallsBackToAClearedContext)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    delegateTo(mainIndex, kSecondTask);

    report(contentCompletion(QStringLiteral("I would rather not.")));

    ASSERT_EQ(m_clients.size(), 4u);
    EXPECT_EQ(sentContents(3), (QStringList { kCoursesPrompt, kSecondTask }));
    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
}

TEST_F(ChatAgentTest, KeptContext_AnEmptySummary_FallsBackToAClearedContext)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    delegateTo(mainIndex, kSecondTask);

    report(contentCompletion(QString()));

    ASSERT_EQ(m_clients.size(), 4u);
    EXPECT_EQ(sentContents(3), (QStringList { kCoursesPrompt, kSecondTask }));
}

TEST_F(ChatAgentTest,
       KeptContext_WhenNoClientCanBeCreatedAfterTheSummary_TheDelegationIsUnavailable)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    delegateTo(mainIndex, kSecondTask);
    m_clientLimit = m_clients.size();

    report(contentCompletion(kSummary));

    EXPECT_TRUE(lastMessageOf(*m_clients.at(mainIndex))
                    .contains(QStringLiteral("The Courses agent failed (unavailable).")));
    EXPECT_EQ(m_events.last(), "end:Delegation courses Failed");
}

TEST_F(ChatAgentTest, KeptContext_APolicyWithoutACompactionRequest_ClearsWhenOverTheLimit)
{
    chat::ContextPolicy policy = keepPolicy();
    policy.compactionRequest.clear();
    keepCoursesContext(policy);
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));

    delegateTo(mainIndex, kSecondTask);

    ASSERT_EQ(m_clients.size(), 3u);
    EXPECT_TRUE(m_clients.at(2)->requested.resume.isEmpty());
    EXPECT_EQ(sentContents(2), (QStringList { kCoursesPrompt, kSecondTask }));
    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
}

TEST_F(ChatAgentTest, KeptContext_ADelegateThatReportsNoTokens_IsOnlyClearedByAge)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, contentCompletion(kFirstReport));

    delegateTo(mainIndex, kSecondTask);

    EXPECT_EQ(sentContents(2), (QStringList { kCoursesPrompt, kTask, kFirstReport, kSecondTask }));
}

TEST_F(ChatAgentTest, KeptContext_ADelegationThatFails_DropsTheKeptContextAndItsSession)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(1000, kFirstReport));
    runDelegation(mainIndex, kSecondTask, failedCompletion(QStringLiteral("server_error")));
    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });

    delegateTo(mainIndex, kThirdTask);

    EXPECT_TRUE(m_clients.at(3)->requested.resume.isEmpty());
    EXPECT_EQ(sentContents(3), (QStringList { kCoursesPrompt, kThirdTask }));
}

TEST_F(ChatAgentTest, KeptContext_ACancelledDelegation_DropsTheKeptContextAndItsSession)
{
    keepCoursesContext();
    const size_t firstTurn = beginTurn();
    runDelegation(firstTurn, kTask, usingTokens(1000, kFirstReport));
    answer(firstTurn);
    const size_t secondTurn = beginTurn();
    delegateTo(secondTurn, kSecondTask);

    m_turn->cancel();

    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
    const size_t thirdTurn = beginTurn();
    delegateTo(thirdTurn, kThirdTask);
    EXPECT_TRUE(m_clients.back()->requested.resume.isEmpty());
    EXPECT_EQ(sentContents(m_clients.size() - 1), (QStringList { kCoursesPrompt, kThirdTask }));
}

TEST_F(ChatAgentTest, KeptContext_ACancelledFirstDelegation_ForgetsItsSession)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    delegateTo(mainIndex);

    m_turn->cancel();

    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
}

TEST_F(ChatAgentTest, KeptContext_ACancelDuringTheCompaction_DropsTheKeptContextAndItsSession)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(kOverTheLimit, kFirstReport));
    delegateTo(mainIndex, kSecondTask);

    m_turn->cancel();

    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
    const size_t nextTurn = beginTurn();
    delegateTo(nextTurn, kThirdTask);
    EXPECT_EQ(sentContents(m_clients.size() - 1), (QStringList { kCoursesPrompt, kThirdTask }));
}

TEST_F(ChatAgentTest, KeptContext_WhenNoClientCanBeCreated_TheKeptContextSurvives)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(1000, kFirstReport));
    m_refuseDelegates = true;
    delegateTo(mainIndex, kSecondTask);
    EXPECT_TRUE(lastMessageOf(*m_clients.at(mainIndex))
                    .contains(QStringLiteral("The Courses agent failed (unavailable).")));
    m_refuseDelegates = false;

    delegateTo(mainIndex, kThirdTask);

    EXPECT_EQ(m_clients.back()->requested.resume, kFirstSession);
    EXPECT_EQ(sentContents(m_clients.size() - 1),
              (QStringList { kCoursesPrompt, kTask, kFirstReport, kThirdTask }));
    EXPECT_TRUE(m_forgotten.isEmpty());
}

TEST_F(ChatAgentTest, KeptContext_ForgettingTheConversation_DropsTheContextAndItsSession)
{
    keepCoursesContext();
    const size_t firstTurn = beginTurn();
    runDelegation(firstTurn, kTask, usingTokens(1000, kFirstReport));
    answer(firstTurn);

    m_backend->forgetConversation(m_conversation);

    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
    const size_t secondTurn = beginTurn();
    delegateTo(secondTurn, kSecondTask);
    EXPECT_TRUE(m_clients.back()->requested.resume.isEmpty());
    EXPECT_EQ(sentContents(m_clients.size() - 1), (QStringList { kCoursesPrompt, kSecondTask }));
}

TEST_F(ChatAgentTest, KeptContext_ForgettingOneConversation_KeepsTheOthers)
{
    keepCoursesContext();
    const QUuid first = m_conversation;
    const size_t firstTurn = beginTurn();
    runDelegation(firstTurn, kTask, usingTokens(1000, kFirstReport));
    answer(firstTurn);
    m_conversation = QUuid::createUuid();
    const size_t otherTurn = beginTurn();
    runDelegation(otherTurn, kTask, usingTokens(1000, kFirstReport));
    answer(otherTurn);

    m_backend->forgetConversation(first);

    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
    const size_t nextTurn = beginTurn();
    delegateTo(nextTurn, kSecondTask);
    EXPECT_EQ(m_clients.back()->requested.resume, kSecondSession);
    EXPECT_EQ(sentContents(m_clients.size() - 1),
              (QStringList { kCoursesPrompt, kTask, kFirstReport, kSecondTask }));
}

TEST_F(ChatAgentTest, KeptContext_DestroyingTheBackend_ForgetsTheKeptSessions)
{
    keepCoursesContext();
    const size_t mainIndex = beginTurn();
    runDelegation(mainIndex, kTask, usingTokens(1000, kFirstReport));
    answer(mainIndex);
    EXPECT_TRUE(m_forgotten.isEmpty());

    m_turn.reset();
    m_backend.reset();

    EXPECT_EQ(m_forgotten, QStringList { kFirstSession });
}
