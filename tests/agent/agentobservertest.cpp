#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include <QJsonDocument>
#include <QStringList>

#include "agent/agent.h"
#include "agent/client.h"
#include "completions.h"

namespace
{

using agent_test::contentCompletion;
using agent_test::failedCompletion;
using agent_test::toolCallCompletion;

const QString kEchoDocstring = R"(@brief echo
@details Returns the text it was given
@param text (string) the text to return)";

const QVariantMap kEchoArguments { { "text", "hi" } };

class HeldClient : public Client
{
public:
    HeldClient()
        : Client("http://mock.invalid", "test-key")
    {
    }

    struct Request
    {
        CompletionCreatedCallback callback;
        PartialTextCallback partialText;
    };

    mutable std::vector<Request> held;
    mutable ToolsMap lastTools;

    void createCompletionAsync(const ModelConfig&, const Messages&, const ToolsMap& toolsMap,
                               const CompletionCreatedCallback& callback,
                               const PartialTextCallback& partialText) const override
    {
        lastTools = toolsMap;
        held.push_back({ callback, partialText });
    }

    void completeNext(Completion completion)
    {
        const Request next = held.front();
        held.erase(held.begin());
        next.callback(completion);
    }

    void streamNext(const QString& textSoFar) const { held.front().partialText(textSoFar); }
};

class FourArgumentClient : public Client
{
public:
    FourArgumentClient()
        : Client("http://mock.invalid", "test-key")
    {
    }

    mutable int calls { 0 };

    void createCompletionAsync(const ModelConfig&, const Messages&, const ToolsMap&,
                               const CompletionCreatedCallback& callback) const override
    {
        ++calls;
        Completion completion = contentCompletion("old style reply");
        callback(completion);
    }
};

QString effectName(ToolEffect effect) { return effect == ToolEffect::Writes ? "writes" : "reads"; }

}

class AgentObserverTest : public ::testing::Test
{
protected:
    AgentObserverTest()
        : agent("gpt-test", "system")
    {
        agent.setObserver(loggingObserver());
    }

    AgentObserver loggingObserver()
    {
        AgentObserver observer;
        observer.toolStarted = [this](const ToolCall& call, ToolEffect effect)
        { events << QStringLiteral("started %1 %2").arg(call.name, effectName(effect)); };
        observer.toolFinished = [this](const ToolCall& call, const QVariant& result)
        { events << QStringLiteral("finished %1 %2").arg(call.name, result.toString()); };
        observer.finished = [this](const QString& answer) { events << "answer " + answer; };
        observer.failed = [this](const QString& token) { events << "failed " + token; };
        return observer;
    }

    Tool echoTool()
    {
        return [this](QVariantMap arguments, ToolDone done)
        {
            ++toolRuns;
            done(arguments.value("text"));
        };
    }

    Tool holdingTool()
    {
        return [this](QVariantMap, ToolDone done)
        {
            ++toolRuns;
            heldDone = done;
        };
    }

    void request(Agent& target)
    {
        target.requestAsync(client, "go",
                            [this](const QString& response) { responses << response; });
    }

    void request() { request(agent); }

    void completeToolCall()
    {
        client.completeNext(toolCallCompletion("c1", "echo", kEchoArguments));
    }

    HeldClient client;
    Agent agent;
    QStringList events;
    QStringList responses;
    QStringList partials;
    int toolRuns { 0 };
    ToolDone heldDone;
};

TEST_F(AgentObserverTest, ToolRound_ReportsToolStartToolFinishAndAnswerInOrder)
{
    agent.addTool(kEchoDocstring, echoTool());
    request();

    completeToolCall();
    client.completeNext(contentCompletion("all done"));

    const QStringList expected { "started echo reads", "finished echo hi", "answer all done" };
    EXPECT_EQ(events, expected);
}

TEST_F(AgentObserverTest, ToolRound_ImageResultKeepsPixelsOutOfTheTextAndOnTheMessage)
{
    const QByteArray pixels("\x89PNG-pixels");
    agent.addTool(kEchoDocstring,
                  [pixels](QVariantMap, ToolDone done)
                  {
                      done(QVariant::fromValue(ToolImage { QStringLiteral("a screen"),
                                                           QStringLiteral("image/png"), pixels }));
                  });
    request();

    completeToolCall();

    const QStringList expected { "started echo reads", "finished echo a screen" };
    EXPECT_EQ(events, expected);

    const Messages& messages = agent.messages();
    const Message& toolMessage = messages.last();
    ASSERT_EQ(toolMessage.role, "tool");
    EXPECT_EQ(toolMessage.content, QStringLiteral("{\"value\":\"a screen\"}"));
    ASSERT_EQ(toolMessage.images.size(), 1);
    EXPECT_EQ(toolMessage.images.first().mimeType, "image/png");
    EXPECT_EQ(toolMessage.images.first().data, pixels);
}

TEST_F(AgentObserverTest, ToolRound_TextResultCarriesNoImages)
{
    agent.addTool(kEchoDocstring, echoTool());
    request();

    completeToolCall();

    EXPECT_TRUE(agent.messages().last().images.isEmpty());
}

TEST_F(AgentObserverTest, ToolRound_KeepsTheResponseCallbackExactlyAsItWas)
{
    agent.addTool(kEchoDocstring, echoTool());
    request();

    completeToolCall();
    client.completeNext(contentCompletion("all done"));

    const QStringList expected { "", "all done" };
    EXPECT_EQ(responses, expected);
}

TEST_F(AgentObserverTest, ToolStarted_FiresBeforeTheToolRunsAndToolFinishedAfterItsDone)
{
    agent.addTool(kEchoDocstring, holdingTool());
    request();

    completeToolCall();

    EXPECT_EQ(toolRuns, 1);
    const QStringList beforeDone { "started echo reads" };
    EXPECT_EQ(events, beforeDone);

    heldDone("the result");

    const QStringList afterDone { "started echo reads", "finished echo the result" };
    EXPECT_EQ(events, afterDone);
}

TEST_F(AgentObserverTest, UnknownTool_IsNeitherStartedNorFinished)
{
    request();

    client.completeNext(toolCallCompletion("c1", "missing", {}));
    client.completeNext(contentCompletion("fine"));

    const QStringList expected { "answer fine" };
    EXPECT_EQ(events, expected);
}

TEST_F(AgentObserverTest, AddTool_WithoutEffect_RecordsReads)
{
    agent.addTool(kEchoDocstring, echoTool());
    request();

    EXPECT_EQ(client.lastTools.value("echo").effect, ToolEffect::Reads);
}

TEST_F(AgentObserverTest, AddTool_WithWrites_RecordsWritesAndToolStartedCarriesIt)
{
    agent.addTool(kEchoDocstring, echoTool(), ToolEffect::Writes);
    request();
    EXPECT_EQ(client.lastTools.value("echo").effect, ToolEffect::Writes);

    completeToolCall();

    EXPECT_EQ(events.first(), "started echo writes");
}

TEST_F(AgentObserverTest, ToolEffect_NeverReachesTheSchemaSentToTheModel)
{
    agent.addTool(kEchoDocstring, echoTool(), ToolEffect::Writes);
    request();

    const QString schema
        = QString::fromUtf8(QJsonDocument(client.lastTools.value("echo").json).toJson());
    EXPECT_FALSE(schema.contains("effect", Qt::CaseInsensitive));
    EXPECT_FALSE(schema.contains("writes", Qt::CaseInsensitive));
}

TEST_F(AgentObserverTest, CompletionError_FailsWithTheTokenAndStillCallsTheErrorCallback)
{
    QStringList errors;
    agent.setErrorCallback([&errors](const QString& error) { errors << error; });
    request();

    client.completeNext(failedCompletion("rate_limit"));

    const QStringList expectedEvents { "failed rate_limit" };
    const QStringList expectedErrors { "rate_limit" };
    const QStringList expectedResponses { "" };
    EXPECT_EQ(events, expectedEvents);
    EXPECT_EQ(errors, expectedErrors);
    EXPECT_EQ(responses, expectedResponses);
}

TEST_F(AgentObserverTest, CompletionWithoutChoices_FailsWithUnknownError)
{
    request();

    client.completeNext(Completion {});

    const QStringList expected { "failed unknown_error" };
    EXPECT_EQ(events, expected);
}

TEST_F(AgentObserverTest, TooManyToolRounds_FailsWithTheLimitTokenOnce)
{
    QStringList errors;
    agent.setErrorCallback([&errors](const QString& error) { errors << error; });
    agent.addTool(kEchoDocstring, echoTool());
    request();

    for (int round = 0; round < 21; ++round)
    {
        completeToolCall();
    }

    EXPECT_EQ(events.last(), "failed too_many_tool_calls");
    EXPECT_EQ(events.count("failed too_many_tool_calls"), 1);
    EXPECT_TRUE(events.filter("answer").isEmpty());
    EXPECT_EQ(toolRuns, 20);
    EXPECT_TRUE(errors.isEmpty());
    EXPECT_TRUE(client.held.empty());
}

TEST_F(AgentObserverTest, TheToolCallLimitDefaultsToTwenty)
{
    EXPECT_EQ(agent.maxToolCalls(), 20);
    EXPECT_EQ(Agent::kDefaultMaxToolCalls, 20);
}

TEST_F(AgentObserverTest, AConfiguredToolCallLimitReplacesTheDefault)
{
    agent.setMaxToolCalls(3);
    agent.addTool(kEchoDocstring, echoTool());
    request();

    for (int round = 0; round < 4; ++round)
    {
        completeToolCall();
    }

    EXPECT_EQ(events.last(), "failed too_many_tool_calls");
    EXPECT_EQ(toolRuns, 3);
    EXPECT_TRUE(client.held.empty());
}

TEST_F(AgentObserverTest, ARaisedToolCallLimitLetsTheAgentGoPastTwenty)
{
    agent.setMaxToolCalls(40);
    agent.addTool(kEchoDocstring, echoTool());
    request();

    for (int round = 0; round < 25; ++round)
    {
        completeToolCall();
    }

    EXPECT_TRUE(events.filter("failed").isEmpty());
    EXPECT_EQ(toolRuns, 25);
    EXPECT_FALSE(client.held.empty());
}

TEST_F(AgentObserverTest, AnUnlimitedToolCallLimitNeverStopsTheAgent)
{
    agent.setMaxToolCalls(Agent::kUnlimitedToolCalls);
    agent.addTool(kEchoDocstring, echoTool());
    request();

    for (int round = 0; round < 100; ++round)
    {
        completeToolCall();
    }

    EXPECT_EQ(agent.maxToolCalls(), Agent::kUnlimitedToolCalls);
    EXPECT_TRUE(events.filter("failed").isEmpty());
    EXPECT_EQ(toolRuns, 100);
    EXPECT_FALSE(client.held.empty());
}

TEST_F(AgentObserverTest, AnUnlimitedAgentStillFinishesWhenTheModelAnswers)
{
    agent.setMaxToolCalls(Agent::kUnlimitedToolCalls);
    agent.addTool(kEchoDocstring, echoTool());
    request();

    for (int round = 0; round < 30; ++round)
    {
        completeToolCall();
    }
    client.completeNext(contentCompletion("done"));

    EXPECT_EQ(events.last(), "answer done");
    EXPECT_EQ(toolRuns, 30);
}

TEST_F(AgentObserverTest, Finish_FiresOnceAndTheAgentAcceptsTheNextRequest)
{
    request();
    client.completeNext(contentCompletion("first"));

    EXPECT_FALSE(agent.asynRequestInProgress());

    request();
    client.completeNext(contentCompletion("second"));

    const QStringList expected { "answer first", "answer second" };
    EXPECT_EQ(events, expected);
}

TEST_F(AgentObserverTest, Cancel_WithAPendingTool_IgnoresTheLateDone)
{
    agent.addTool(kEchoDocstring, holdingTool());
    request();
    completeToolCall();

    agent.cancel();
    heldDone("late result");

    const QStringList expected { "started echo reads" };
    EXPECT_EQ(events, expected);
    EXPECT_TRUE(client.held.empty());
    EXPECT_TRUE(responses.isEmpty());
    EXPECT_FALSE(agent.asynRequestInProgress());
}

TEST_F(AgentObserverTest, Cancel_WithAPendingCompletion_IgnoresTheLateCompletion)
{
    request();

    agent.cancel();
    client.completeNext(contentCompletion("late"));

    EXPECT_TRUE(events.isEmpty());
    EXPECT_TRUE(responses.isEmpty());
    EXPECT_EQ(agent.messages().size(), 2);
    EXPECT_FALSE(agent.asynRequestInProgress());
}

TEST_F(AgentObserverTest, Cancel_ThenANewRequest_RunsWhileTheStaleCompletionStaysIgnored)
{
    request();
    agent.cancel();
    request();
    ASSERT_EQ(client.held.size(), 2u);

    client.completeNext(contentCompletion("stale"));
    client.completeNext(contentCompletion("fresh"));

    const QStringList expected { "answer fresh" };
    EXPECT_EQ(events, expected);
}

TEST_F(AgentObserverTest, Cancel_FromToolStarted_StopsBeforeTheToolRuns)
{
    AgentObserver observer = loggingObserver();
    observer.toolStarted = [this](const ToolCall&, ToolEffect) { agent.cancel(); };
    agent.setObserver(observer);
    agent.addTool(kEchoDocstring, echoTool());
    request();

    completeToolCall();

    EXPECT_EQ(toolRuns, 0);
    EXPECT_TRUE(client.held.empty());
    EXPECT_TRUE(events.isEmpty());
}

TEST_F(AgentObserverTest, Cancel_FromToolFinished_StopsTheLoop)
{
    AgentObserver observer = loggingObserver();
    observer.toolFinished = [this](const ToolCall&, const QVariant&) { agent.cancel(); };
    agent.setObserver(observer);
    agent.addTool(kEchoDocstring, echoTool());
    request();

    completeToolCall();

    EXPECT_EQ(toolRuns, 1);
    EXPECT_TRUE(client.held.empty());
    EXPECT_FALSE(agent.asynRequestInProgress());
}

TEST_F(AgentObserverTest, Cancel_FromTheEmptyToolRoundCallback_SendsNoFurtherCompletion)
{
    agent.addTool(kEchoDocstring, echoTool());
    agent.requestAsync(client, "go", [this](const QString&) { agent.cancel(); });

    completeToolCall();

    EXPECT_EQ(toolRuns, 1);
    EXPECT_TRUE(client.held.empty());
    EXPECT_FALSE(agent.asynRequestInProgress());
}

TEST_F(AgentObserverTest, Destruction_WithAPendingCompletion_IgnoresTheLateCompletion)
{
    auto doomed = std::make_unique<Agent>("gpt-test", "system");
    doomed->setObserver(loggingObserver());
    request(*doomed);

    doomed.reset();
    client.completeNext(contentCompletion("late"));

    EXPECT_TRUE(events.isEmpty());
    EXPECT_TRUE(responses.isEmpty());
}

TEST_F(AgentObserverTest, Destruction_WithAPendingTool_IgnoresTheLateDone)
{
    auto doomed = std::make_unique<Agent>("gpt-test", "system");
    doomed->setObserver(loggingObserver());
    doomed->addTool(kEchoDocstring, holdingTool());
    request(*doomed);
    completeToolCall();

    doomed.reset();
    heldDone("late result");

    const QStringList expected { "started echo reads" };
    EXPECT_EQ(events, expected);
    EXPECT_TRUE(client.held.empty());
}

TEST_F(AgentObserverTest, CopiedAgent_DestroyedWithAPendingCompletion_IgnoresTheLateCompletion)
{
    auto copy = std::make_unique<Agent>(agent);
    request(*copy);

    copy.reset();
    client.completeNext(contentCompletion("late"));

    EXPECT_TRUE(events.isEmpty());
}

TEST_F(AgentObserverTest, CopiedAgent_IsNotCancelledByCancellingTheOriginal)
{
    Agent copy(agent);
    request(copy);

    agent.cancel();
    client.completeNext(contentCompletion("from the copy"));

    const QStringList expected { "answer from the copy" };
    EXPECT_EQ(events, expected);
}

TEST_F(AgentObserverTest, AssignedAgent_WithAPendingCompletion_IgnoresTheLateCompletion)
{
    Agent replacement("gpt-test", "replacement system");
    request();

    agent = replacement;
    client.completeNext(contentCompletion("late"));

    EXPECT_TRUE(events.isEmpty());
    EXPECT_TRUE(responses.isEmpty());
    EXPECT_EQ(agent.messages().size(), 1);
    EXPECT_FALSE(agent.asynRequestInProgress());
}

TEST_F(AgentObserverTest, PartialText_ReachesTheCallbackInOrder)
{
    agent.setPartialTextCallback([this](const QString& text) { partials << text; });
    request();

    client.streamNext("Hel");
    client.streamNext("Hello");
    client.completeNext(contentCompletion("Hello"));

    const QStringList expected { "Hel", "Hello" };
    EXPECT_EQ(partials, expected);
}

TEST_F(AgentObserverTest, PartialText_WithoutACallback_IsIgnored)
{
    request();

    client.streamNext("Hel");
    client.completeNext(contentCompletion("Hello"));

    const QStringList expected { "answer Hello" };
    EXPECT_EQ(events, expected);
}

TEST_F(AgentObserverTest, PartialText_AfterCancel_IsDropped)
{
    agent.setPartialTextCallback([this](const QString& text) { partials << text; });
    request();

    agent.cancel();
    client.streamNext("late");

    EXPECT_TRUE(partials.isEmpty());
}

TEST_F(AgentObserverTest, ClientThatOnlyKnowsTheOldOverload_StillAnswersWithoutPartialText)
{
    FourArgumentClient oldClient;
    agent.setPartialTextCallback([this](const QString& text) { partials << text; });

    agent.requestAsync(oldClient, "go", [this](const QString& response) { responses << response; });

    const QStringList expected { "answer old style reply" };
    EXPECT_EQ(events, expected);
    EXPECT_EQ(oldClient.calls, 1);
    EXPECT_TRUE(partials.isEmpty());
}

TEST_F(AgentObserverTest, ClientWithoutASession_ReportsAnEmptySessionId)
{
    FourArgumentClient plainClient;

    EXPECT_TRUE(plainClient.session().isEmpty());
}
