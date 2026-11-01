#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPair>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

#include "agent/agent.h"
#include "agent/claudecli.h"
#include "agent/claudestdioclient.h"
#include "claudestdiosupport.h"
#include "environmentoverride.h"
#include "toolsmapof.h"

using namespace stdiotest;

namespace
{

const QString kCoursesRequestId = QStringLiteral("e51d362f-a1da-49c0-b955-479f5750c498");

using ToolResults = QList<QPair<QString, QString>>;

QStringList slowDeltas(int count, int pauseMs)
{
    QStringList lines;
    for (int i = 0; i < count; ++i)
    {
        lines << cliSays(textDeltaEvent(QString::number(i)));
        lines << cliRuns(QStringLiteral("sleep %1").arg(pauseMs));
    }
    return lines;
}

Messages afterTools(Messages messages, const ToolResults& results)
{
    for (const auto& [callId, content] : results)
        messages.append(toolMessage(callId, content));
    return messages;
}

void writeExecutable(const QString& path)
{
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\n");
    file.close();
    ASSERT_TRUE(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));
}

}

TEST(ClaudeStdioClientTest, AnswersWithTheResultOfTheRun)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_TRUE(outcome.error().isEmpty());
    ASSERT_EQ(outcome.completion.choices.size(), 1);
    EXPECT_EQ(outcome.completion.choices.first().message.role, "assistant");
    EXPECT_EQ(outcome.completion.choices.first().message.content, "pong");
    EXPECT_EQ(outcome.finishReason(), "stop");
    EXPECT_TRUE(outcome.toolCalls().isEmpty());
}

TEST(ClaudeStdioClientTest, StartsTheCliInStreamJsonModeAndPersistsNothing)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());

    ask(client, conversationOf("ping"));

    EXPECT_TRUE(claude.hasArgument("-p"));
    EXPECT_EQ(claude.valueOf("--input-format"), "stream-json");
    EXPECT_EQ(claude.valueOf("--output-format"), "stream-json");
    EXPECT_TRUE(claude.hasArgument("--verbose"));
    EXPECT_TRUE(claude.hasArgument("--include-partial-messages"));
    EXPECT_EQ(claude.valueOf("--system-prompt-snapshot"), "off");
    EXPECT_TRUE(claude.hasArgument("--no-session-persistence"));
    EXPECT_FALSE(claude.hasArgument("--resume"));
    EXPECT_FALSE(claude.hasArgument("--session-id"));
    EXPECT_EQ(claude.valueOf("--system-prompt"), "You are a test.");
    EXPECT_TRUE(claude.hasArgument("--restricted"));
    ASSERT_TRUE(claude.hasArgument("--tools"));
    EXPECT_TRUE(claude.valueOf("--tools").isEmpty());
    EXPECT_EQ(claude.valueOf("--permission-mode"), "dontAsk");
    EXPECT_EQ(claude.valueOf("--permission-prompts"), "none");
}

TEST(ClaudeStdioClientTest, StartsTheCliOncePerRun)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());

    ask(client, conversationOf("ping"));

    EXPECT_EQ(claude.startCount(), 1);
}

TEST(ClaudeStdioClientTest, OffersNoToolServerWhenThereAreNoTools)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());

    ask(client, conversationOf("ping"));

    EXPECT_FALSE(claude.hasArgument("--mcp-config"));
    EXPECT_FALSE(claude.hasArgument("--strict-mcp-config"));
    EXPECT_FALSE(claude.hasArgument("--allowedTools"));
}

TEST(ClaudeStdioClientTest, OffersTheToolsThroughTheSdkServerOnly)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());

    ask(client, conversationOf("ping"), toolsMapOf({ "alpha" }));

    const QJsonObject config
        = QJsonDocument::fromJson(claude.valueOf("--mcp-config").toUtf8()).object();
    const QJsonObject server = config.value("mcpServers").toObject().value("app").toObject();
    EXPECT_EQ(server.value("type").toString(), "sdk");
    EXPECT_EQ(server.value("name").toString(), "app");
    EXPECT_EQ(config.value("mcpServers").toObject().size(), 1);
    EXPECT_TRUE(claude.hasArgument("--strict-mcp-config"));
    EXPECT_EQ(claude.valueOf("--allowedTools"), "mcp__app");
    EXPECT_TRUE(claude.valueOf("--tools").isEmpty());
}

TEST(ClaudeStdioClientTest, PassesTheModelAndTheReasoningEffortToTheCli)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());

    ask(client, conversationOf("ping"), {}, ModelConfig { "sonnet[1m]", "high" });

    EXPECT_EQ(claude.valueOf("--model"), "sonnet[1m]");
    EXPECT_EQ(claude.valueOf("--effort"), "high");
}

TEST(ClaudeStdioClientTest, FallsBackToHaikuAndOmitsAnEffortTheCliDoesNotKnow)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());

    ask(client, conversationOf("ping"), {}, ModelConfig { "gpt-4o", "turbo" });

    EXPECT_EQ(claude.valueOf("--model"), "haiku");
    EXPECT_FALSE(claude.hasArgument("--effort"));
}

TEST(ClaudeStdioClientTest, WritesTheInitializeRequestAndTheConversationToTheCli)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());
    Messages messages = conversationOf("What is 2+2?");
    messages.append(messageOf("assistant", "4"));
    messages.append(messageOf("user", "And 3+3?"));

    ask(client, messages);

    const QList<QJsonObject> inputs = claude.inputs();
    ASSERT_EQ(inputs.size(), 2);
    EXPECT_EQ(inputs[0].value("type").toString(), "control_request");
    EXPECT_EQ(inputs[0].value("request").toObject().value("subtype").toString(), "initialize");
    EXPECT_EQ(inputs[1].value("type").toString(), "user");
    EXPECT_EQ(userTextOf(inputs[1]), claudecli::conversation(messages));
    EXPECT_TRUE(userTextOf(inputs[1]).contains("And 3+3?"));
}

TEST(ClaudeStdioClientTest, RunsTheCliInTheConfiguredDirectory)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());

    ask(client, conversationOf("ping"));

    EXPECT_EQ(QFileInfo(claude.startDirectory()).canonicalFilePath(),
              QFileInfo(claude.workingDirectory()).canonicalFilePath());
}

TEST(ClaudeStdioClientTest, GivesTheCliExactlyTheEnvironmentOfTheLauncher)
{
    const FakeClaude claude(startOfRun()
                            + QStringList { cliRuns("env FAKE_CLAUDE_MARKER"), cliRuns("env HOME"),
                                            cliSays(resultEvent("pong")) });
    ClaudeLauncher launcher = claude.launcher();
    launcher.environment.insert("FAKE_CLAUDE_MARKER", "from-launcher");
    const ClaudeStdioClient client(launcher);

    ask(client, conversationOf("ping"));

    const QList<QJsonObject> variables = claude.recordsWith("env");
    ASSERT_EQ(variables.size(), 2);
    EXPECT_EQ(variables[0].value("value").toString(), "from-launcher");
    EXPECT_TRUE(variables[1].value("value").isNull());
}

TEST(ClaudeStdioClientTest, FailsWithAServerErrorWhenTheRunEndsWithAnError)
{
    const FakeClaude claude(startOfRun() + QStringList { cliSays(resultEvent("boom", true)) });
    const ClaudeStdioClient client(claude.launcher());

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.error(), claudecli::kServerError);
    EXPECT_TRUE(outcome.completion.choices.isEmpty());
}

TEST(ClaudeStdioClientTest, FailsWithNotLoggedInWhenTheCliIsNotLoggedIn)
{
    const FakeClaude claude = FakeClaude::replaying("stdio-not-logged-in.jsonl");
    const ClaudeStdioClient client(claude.launcher());

    const Outcome outcome = ask(client, conversationOf("ping"), toolsMapOf({ "courses_list" }));

    EXPECT_EQ(outcome.error(), claudecli::kNotLoggedIn);
    EXPECT_TRUE(outcome.completion.choices.isEmpty());
}

TEST(ClaudeStdioClientTest, FailsWithAServerErrorWhenTheCliCannotFindItsSession)
{
    const FakeClaude claude = FakeClaude::replaying("stdio-missing-session.jsonl");
    const ClaudeStdioClient client(claude.launcher());

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.error(), claudecli::kServerError);
}

TEST(ClaudeStdioClientTest, FailsWithAServerErrorWhenTheCliExitsWithoutAResult)
{
    const FakeClaude claude(startOfRun() + QStringList { cliRuns("exit 1") });
    const ClaudeStdioClient client(claude.launcher());

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.error(), claudecli::kServerError);
}

TEST(ClaudeStdioClientTest, FailsWithAServerErrorWhenTheCliCrashes)
{
    const FakeClaude claude(startOfRun() + QStringList { cliRuns("crash") });
    const ClaudeStdioClient client(claude.launcher());

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.error(), claudecli::kServerError);
}

TEST(ClaudeStdioClientTest, FailsWithANetworkErrorWhenTheProgramCannotBeStarted)
{
    const FakeClaude claude(answering("pong"));
    ClaudeLauncher launcher = claude.launcher();
    launcher.program = QDir(claude.workingDirectory()).filePath("missing-program");
    const ClaudeStdioClient client(launcher);

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.error(), claudecli::kNetworkError);
}

TEST(ClaudeStdioClientTest, FailsWithANetworkErrorWhenThereIsNoProgram)
{
    const FakeClaude claude(answering("pong"));
    ClaudeLauncher launcher = claude.launcher();
    launcher.program.clear();
    const ClaudeStdioClient client(launcher);

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.error(), claudecli::kNetworkError);
    EXPECT_EQ(claude.startCount(), 0);
}

TEST(ClaudeStdioClientTest, ReportsTheGrowingTextBeforeTheAnswer)
{
    const FakeClaude claude(
        startOfRun()
        + QStringList { cliSays(messageStartEvent()), cliSays(textDeltaEvent("Hel")),
                        cliSays(textDeltaEvent("lo")), cliSays(resultEvent("Hello")) });
    const ClaudeStdioClient client(claude.launcher());

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.partials, QStringList({ "Hel", "Hello" }));
    EXPECT_EQ(outcome.events, QStringList({ "partial:Hel", "partial:Hello", "done" }));
    EXPECT_EQ(outcome.text(), "Hello");
}

TEST(ClaudeStdioClientTest, StartsTheGrowingTextOverWithEveryMessage)
{
    const FakeClaude claude(
        startOfRun()
        + QStringList { cliSays(messageStartEvent()), cliSays(textDeltaEvent("First")),
                        cliSays(messageStartEvent()), cliSays(textDeltaEvent("Second")),
                        cliSays(resultEvent("Second")) });
    const ClaudeStdioClient client(claude.launcher());

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.partials, QStringList({ "First", "Second" }));
}

TEST(ClaudeStdioClientTest, AnswersWithoutAPartialTextCallback)
{
    const FakeClaude claude(
        startOfRun()
        + QStringList { cliSays(textDeltaEvent("Hel")), cliSays(resultEvent("Hello")) });
    const ClaudeStdioClient client(claude.launcher());
    Completion completion;
    bool done = false;

    client.createCompletionAsync(defaultConfig(), conversationOf("ping"), {},
                                 [&completion, &done](Completion& received)
                                 {
                                     completion = received;
                                     done = true;
                                 });

    ASSERT_TRUE(QTest::qWaitFor([&done] { return done; }, kWaitMs));
    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.first().message.content, "Hello");
}

TEST(ClaudeStdioClientTest, ReturnsTheCompletionFromTheSynchronousCall)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());

    const Completion completion
        = client.createCompletion(defaultConfig(), conversationOf("ping"), {});

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.first().message.content, "pong");
}

TEST(ClaudeStdioClientTest, ReturnsTheFailureFromTheSynchronousCall)
{
    const FakeClaude claude(startOfRun() + QStringList { cliRuns("exit 1") });
    const ClaudeStdioClient client(claude.launcher());

    const Completion completion
        = client.createCompletion(defaultConfig(), conversationOf("ping"), {});

    EXPECT_EQ(completion.error, claudecli::kServerError);
}

TEST(ClaudeStdioClientTest, HandsOverAParallelBurstOfToolCallsAsOneCompletion)
{
    const FakeClaude claude(
        startOfRun()
        + QStringList { cliBuffers(toolCallEvent("req-a", 2, "alpha", QJsonObject { { "n", 5 } })),
                        cliSays(toolCallEvent("req-b", 3, "beta")), ">", ">",
                        cliSays(resultEvent("both done")) });
    const ClaudeStdioClient client(claude.launcher());
    const ToolsMap tools = toolsMapOf({ "alpha", "beta" });
    const Messages messages = conversationOf("go");

    const std::shared_ptr<Outcome> first = submit(client, messages, tools);
    ASSERT_TRUE(waitDone(first));

    EXPECT_EQ(first->finishReason(), "tool_calls");
    EXPECT_TRUE(first->text().isEmpty());
    ASSERT_EQ(first->toolCalls().size(), 2);
    EXPECT_EQ(first->toolCalls()[0].id, "req-a");
    EXPECT_EQ(first->toolCalls()[0].name, "alpha");
    EXPECT_EQ(first->toolCalls()[0].type, "function");
    EXPECT_EQ(first->toolCalls()[0].arguments.value("n").toInt(), 5);
    EXPECT_EQ(first->toolCalls()[1].id, "req-b");
    EXPECT_EQ(first->toolCalls()[1].name, "beta");

    const Outcome second
        = ask(client, afterTools(messages, { { "req-a", "A!" }, { "req-b", "B!" } }), tools);

    EXPECT_EQ(second.text(), "both done");
    EXPECT_EQ(claude.startCount(), 1);
}

TEST(ClaudeStdioClientTest, AnswersEveryToolCallUnderItsOwnRequestId)
{
    const FakeClaude claude(startOfRun()
                            + QStringList { cliBuffers(toolCallEvent("req-a", 2, "alpha")),
                                            cliSays(toolCallEvent("req-b", 3, "beta")), ">", ">",
                                            cliSays(resultEvent("both done")) });
    const ClaudeStdioClient client(claude.launcher());
    const ToolsMap tools = toolsMapOf({ "alpha", "beta" });
    const Messages messages = conversationOf("go");
    const std::shared_ptr<Outcome> first = submit(client, messages, tools);
    ASSERT_TRUE(waitDone(first));

    ask(client, afterTools(messages, { { "req-b", "B!" }, { "req-a", "A!" } }), tools);

    const QList<QJsonObject> inputs = claude.inputs();
    ASSERT_EQ(inputs.size(), 4);
    EXPECT_EQ(requestIdOf(inputs[2]), "req-a");
    EXPECT_EQ(mcpReplyOf(inputs[2]).value("id").toInt(), 2);
    EXPECT_EQ(mcpReplyOf(inputs[2])
                  .value("result")
                  .toObject()
                  .value("content")
                  .toArray()
                  .first()
                  .toObject()
                  .value("text")
                  .toString(),
              "A!");
    EXPECT_EQ(requestIdOf(inputs[3]), "req-b");
    EXPECT_EQ(mcpReplyOf(inputs[3]).value("id").toInt(), 3);
}

TEST(ClaudeStdioClientTest, QueuesAToolCallThatArrivesWhileTheOthersAreAwaitingAnswers)
{
    const FakeClaude claude(
        startOfRun()
        + QStringList { cliSays(toolCallEvent("req-a", 2, "alpha")), cliRuns("sleep 200"),
                        cliSays(toolCallEvent("req-b", 3, "beta")), cliRuns("env READY"), ">", ">",
                        cliSays(resultEvent("both done")) });
    const ClaudeStdioClient client(claude.launcher());
    const ToolsMap tools = toolsMapOf({ "alpha", "beta" });
    const Messages messages = conversationOf("go");

    const std::shared_ptr<Outcome> first = submit(client, messages, tools);
    ASSERT_TRUE(waitDone(first));
    ASSERT_EQ(first->toolCalls().size(), 1);
    ASSERT_TRUE(claude.waitForRecord("env"));
    QTest::qWait(300);

    const std::shared_ptr<Outcome> second
        = submit(client, afterTools(messages, { { "req-a", "A!" } }), tools);

    ASSERT_TRUE(second->done);
    EXPECT_EQ(second->finishReason(), "tool_calls");
    ASSERT_EQ(second->toolCalls().size(), 1);
    EXPECT_EQ(second->toolCalls()[0].id, "req-b");

    const Outcome third = ask(client, afterTools(messages, { { "req-b", "B!" } }), tools);

    EXPECT_EQ(third.text(), "both done");
    EXPECT_EQ(claude.startCount(), 1);
}

TEST(ClaudeStdioClientTest, FailsAndKillsTheCliWhenAToolAnswerIsMissing)
{
    const FakeClaude claude(startOfRun()
                            + QStringList { cliSays(toolCallEvent("req-a", 2, "alpha")), ">",
                                            cliSays(resultEvent("never")) });
    const ClaudeStdioClient client(claude.launcher());
    const ToolsMap tools = toolsMapOf({ "alpha" });
    const Messages messages = conversationOf("go");
    const std::shared_ptr<Outcome> first = submit(client, messages, tools);
    ASSERT_TRUE(waitDone(first));

    const Outcome second = ask(client, afterTools(messages, { { "other", "A!" } }), tools);

    EXPECT_EQ(second.error(), claudecli::kServerError);
    EXPECT_TRUE(waitUntilDead(claude.processId()));
}

TEST(ClaudeStdioClientTest, FailsTheNextAskWhenTheCliDiesWhileTheToolsAreAwaitingAnswers)
{
    const FakeClaude claude(
        startOfRun()
        + QStringList { cliSays(toolCallEvent("req-a", 2, "alpha")), cliRuns("exit 0") });
    const ClaudeStdioClient client(claude.launcher());
    const ToolsMap tools = toolsMapOf({ "alpha" });
    const Messages messages = conversationOf("go");
    const std::shared_ptr<Outcome> first = submit(client, messages, tools);
    ASSERT_TRUE(waitDone(first));
    ASSERT_TRUE(waitUntilDead(claude.processId()));
    QTest::qWait(300);

    const std::shared_ptr<Outcome> second
        = submit(client, afterTools(messages, { { "req-a", "A!" } }), tools);

    ASSERT_TRUE(second->done);
    EXPECT_EQ(second->error(), claudecli::kServerError);
}

TEST(ClaudeStdioClientTest, AnswersTheRecordedToolRunThroughTheAgent)
{
    const FakeClaude claude = FakeClaude::replaying("stdio-tool-run.jsonl");
    const ClaudeStdioClient client(claude.launcher());
    Agent agent("haiku", "You are a test.");
    int toolCalls = 0;
    agent.addTool(kCoursesListDocstring,
                  [&toolCalls](QVariantMap, ToolDone done)
                  {
                      ++toolCalls;
                      done(kCourseTitles);
                  });

    const QString response = requestThroughAgent(agent, client, "Which courses do I have?");

    EXPECT_EQ(response, "Your courses:\n\n- Linux Kernel Basics\n- C++ Memory Model");
    EXPECT_EQ(toolCalls, 1);
    EXPECT_EQ(agent.messages().size(), 5);
    EXPECT_EQ(claude.startCount(), 1);
}

TEST(ClaudeStdioClientTest, AnswersTheToolRequestsOfTheRecordedRunUnderTheirIds)
{
    const FakeClaude claude = FakeClaude::replaying("stdio-tool-run.jsonl");
    const ClaudeStdioClient client(claude.launcher());
    Agent agent("haiku", "You are a test.");
    agent.addTool(kCoursesListDocstring, [](QVariantMap, ToolDone done) { done(kCourseTitles); });

    requestThroughAgent(agent, client, "Which courses do I have?");

    const QList<QJsonObject> inputs = claude.inputs();
    ASSERT_EQ(inputs.size(), 6);
    const QJsonObject toolsList = mcpReplyOf(inputs[4]).value("result").toObject();
    ASSERT_EQ(toolsList.value("tools").toArray().size(), 1);
    EXPECT_EQ(toolsList.value("tools").toArray().first().toObject().value("name").toString(),
              "courses_list");

    EXPECT_EQ(requestIdOf(inputs[5]), kCoursesRequestId);
    const QJsonObject reply = mcpReplyOf(inputs[5]);
    EXPECT_EQ(reply.value("id").toInt(), 2);
    const QJsonObject result = reply.value("result").toObject();
    EXPECT_FALSE(result.value("isError").toBool());
    const QString text
        = result.value("content").toArray().first().toObject().value("text").toString();
    EXPECT_EQ(QJsonDocument::fromJson(text.toUtf8()).object().value("value").toString(),
              kCourseTitles);
}

TEST(ClaudeStdioClientTest, StreamsTheTextOfTheRecordedRunThroughTheAgent)
{
    const FakeClaude claude = FakeClaude::replaying("stdio-partial-run.jsonl");
    const ClaudeStdioClient client(claude.launcher());
    Agent agent("haiku", "You are a test.");
    agent.addTool(kCoursesListDocstring, [](QVariantMap, ToolDone done) { done(kCourseTitles); });
    QStringList partials;
    agent.setPartialTextCallback([&partials](const QString& text) { partials << text; });

    const QString response = requestThroughAgent(agent, client, "Which courses do I have?");

    ASSERT_EQ(partials.size(), 23);
    EXPECT_EQ(partials[0], "Checking your");
    EXPECT_EQ(partials[1], "Checking your courses now.");
    EXPECT_EQ(partials[2], "You");
    EXPECT_EQ(partials.last(), response);
    EXPECT_TRUE(response.startsWith("You have two courses:"));
}

TEST(ClaudeStdioClientTest, TimesOutAndKillsTheWholeProcessGroupWhenTheCliGoesSilent)
{
    const FakeClaude claude(startOfRun() + QStringList { cliRuns("spawn-child"), cliRuns("hang") });
    ClaudeLauncher launcher = claude.launcher();
    launcher.idleTimeoutMs = 300;
    const ClaudeStdioClient client(launcher);

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.error(), claudecli::kTimeout);
    ASSERT_TRUE(claude.waitForRecord("child"));
    EXPECT_TRUE(waitUntilDead(claude.processId()));
    EXPECT_TRUE(waitUntilDead(claude.recordWith("child").value("child").toInteger()));
}

TEST(ClaudeStdioClientTest, TimesOutWhenTheRunLastsLongerThanTheHardLimit)
{
    const FakeClaude claude(startOfRun() + slowDeltas(30, 100));
    ClaudeLauncher launcher = claude.launcher();
    launcher.hardTimeoutMs = 600;
    const ClaudeStdioClient client(launcher);

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.error(), claudecli::kTimeout);
    EXPECT_TRUE(waitUntilDead(claude.processId()));
}

TEST(ClaudeStdioClientTest, KeepsRunningWhileTheCliKeepsProducingOutput)
{
    const FakeClaude claude(startOfRun() + slowDeltas(6, 150)
                            + QStringList { cliSays(resultEvent("late")) });
    ClaudeLauncher launcher = claude.launcher();
    launcher.idleTimeoutMs = 400;
    const ClaudeStdioClient client(launcher);

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_TRUE(outcome.error().isEmpty());
    EXPECT_EQ(outcome.text(), "late");
}

TEST(ClaudeStdioClientTest, DoesNotCountTheWaitForToolAnswersAsSilence)
{
    const FakeClaude claude(startOfRun()
                            + QStringList { cliSays(toolCallEvent("req-a", 2, "alpha")), ">",
                                            cliSays(resultEvent("after the wait")) });
    ClaudeLauncher launcher = claude.launcher();
    launcher.idleTimeoutMs = 300;
    const ClaudeStdioClient client(launcher);
    const ToolsMap tools = toolsMapOf({ "alpha" });
    const Messages messages = conversationOf("go");
    const std::shared_ptr<Outcome> first = submit(client, messages, tools);
    ASSERT_TRUE(waitDone(first));

    QTest::qWait(700);
    const Outcome second = ask(client, afterTools(messages, { { "req-a", "A!" } }), tools);

    EXPECT_TRUE(second.error().isEmpty());
    EXPECT_EQ(second.text(), "after the wait");
}

TEST(ClaudeStdioClientTest, KillsACliThatLingersAfterTheAnswerWhenTheHardLimitPasses)
{
    const FakeClaude claude(answering("pong") + QStringList { cliRuns("hang") });
    ClaudeLauncher launcher = claude.launcher();
    launcher.hardTimeoutMs = 500;
    const ClaudeStdioClient client(launcher);

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.text(), "pong");
    EXPECT_TRUE(waitUntilDead(claude.processId()));
}

TEST(ClaudeStdioClientTest, KillsTheWholeProcessGroupWhenTheClientIsDestroyed)
{
    const FakeClaude claude(startOfRun() + QStringList { cliRuns("spawn-child"), cliRuns("hang") });
    auto client = std::make_unique<ClaudeStdioClient>(claude.launcher());
    const std::shared_ptr<Outcome> outcome = submit(*client, conversationOf("ping"));
    ASSERT_TRUE(claude.waitForRecord("child"));
    const qint64 child = claude.recordWith("child").value("child").toInteger();
    ASSERT_TRUE(isAlive(claude.processId()));
    ASSERT_TRUE(isAlive(child));

    client.reset();

    EXPECT_TRUE(waitUntilDead(claude.processId()));
    EXPECT_TRUE(waitUntilDead(child));
    EXPECT_FALSE(outcome->done);
}

TEST(ClaudeStdioClientTest, StartsTheCliAsTheLeaderOfItsOwnSessionThatDiesWithTheApplication)
{
    const FakeClaude claude(startOfRun()
                            + QStringList { cliRuns("identity"), cliSays(resultEvent("pong")) });
    const ClaudeStdioClient client(claude.launcher());

    ask(client, conversationOf("ping"));

    const QJsonObject identity = claude.recordWith("sid");
    const qint64 pid = claude.processId();
    EXPECT_EQ(identity.value("pid").toInteger(), pid);
    EXPECT_EQ(identity.value("sid").toInteger(), pid);
    EXPECT_EQ(identity.value("pgid").toInteger(), pid);
    EXPECT_EQ(identity.value("pdeathsig").toInt(), 9);
}

TEST(ClaudeStdioClientTest, ClosesTheInputOfTheCliAfterTheAnswer)
{
    const FakeClaude claude(answering("pong") + QStringList { cliRuns("eof") });
    const ClaudeStdioClient client(claude.launcher());

    const Outcome outcome = ask(client, conversationOf("ping"));

    EXPECT_EQ(outcome.text(), "pong");
    EXPECT_TRUE(claude.waitForRecord("eof"));
}

TEST(ClaudeStdioClientTest, FailsAnAskOnAFinishedRun)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher());
    ask(client, conversationOf("ping"));

    const Outcome second = ask(client, conversationOf("again"));

    EXPECT_EQ(second.error(), claudecli::kServerError);
    EXPECT_EQ(claude.startCount(), 1);
}

TEST(ClaudeStdioClientTest, FailsAnAskWhileTheRunIsStillWorkingAndLetsTheRunFinish)
{
    const FakeClaude claude(startOfRun()
                            + QStringList { cliRuns("sleep 300"), cliSays(resultEvent("pong")) });
    const ClaudeStdioClient client(claude.launcher());

    const std::shared_ptr<Outcome> first = submit(client, conversationOf("ping"));
    const std::shared_ptr<Outcome> second = submit(client, conversationOf("again"));

    ASSERT_TRUE(second->done);
    EXPECT_EQ(second->error(), claudecli::kServerError);
    EXPECT_FALSE(first->done);
    ASSERT_TRUE(waitDone(first));
    EXPECT_EQ(first->text(), "pong");
    EXPECT_EQ(claude.startCount(), 1);
}

TEST(ClaudeStdioClientTest, SurvivesAClientDestroyedFromTheCompletionCallback)
{
    const FakeClaude claude(answering("pong") + QStringList { cliRuns("hang") });
    auto client = std::make_unique<ClaudeStdioClient>(claude.launcher());
    bool done = false;

    client->createCompletionAsync(defaultConfig(), conversationOf("ping"), {},
                                  [&client, &done](Completion&)
                                  {
                                      client.reset();
                                      done = true;
                                  });

    ASSERT_TRUE(QTest::qWaitFor([&done] { return done; }, kWaitMs));
    EXPECT_FALSE(client);
    EXPECT_TRUE(waitUntilDead(claude.processId()));
}

TEST(ClaudeStdioClientTest, SurvivesAClientDestroyedFromThePartialTextCallback)
{
    const FakeClaude claude(startOfRun()
                            + QStringList { cliSays(textDeltaEvent("Hel")),
                                            cliSays(textDeltaEvent("lo")), cliRuns("hang") });
    auto client = std::make_unique<ClaudeStdioClient>(claude.launcher());
    bool completed = false;
    int partials = 0;

    client->createCompletionAsync(
        defaultConfig(), conversationOf("ping"), {},
        [&completed](Completion&) { completed = true; },
        [&client, &partials](const QString&)
        {
            ++partials;
            client.reset();
        });

    ASSERT_TRUE(waitUntilDead(claude.processId(), kWaitMs));
    QTest::qWait(200);
    EXPECT_EQ(partials, 1);
    EXPECT_FALSE(completed);
}

TEST(ClaudeStdioClientTest, IsAvailableWhenTheClaudeExecutableIsOnThePath)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    writeExecutable(QDir(directory.path()).filePath("claude"));
    const EnvironmentOverride path("PATH", directory.path().toUtf8());

    EXPECT_TRUE(ClaudeStdioClient::isAvailable());
}

TEST(ClaudeStdioClientTest, IsUnavailableWhenThePathHasNoClaudeExecutable)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const EnvironmentOverride path("PATH", directory.path().toUtf8());

    EXPECT_FALSE(ClaudeStdioClient::isAvailable());
}
