#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "chat/application/agentbackend.h"
#include "chatclaude/streamjsonparser.h"
#include "fixturefile.h"

using namespace chat;
using namespace chat::claude;
using chat::test::readFixture;

namespace
{

std::string statusName(StepStatus status)
{
    switch (status)
    {
        case StepStatus::Running:
            return "running";
        case StepStatus::Ok:
            return "ok";
        case StepStatus::Failed:
            return "failed";
    }
    return {};
}

std::string label(const Step& step)
{
    QStringList parts { step.kind == StepKind::Delegation ? QStringLiteral("delegation")
                                                          : QStringLiteral("tool") };
    parts << step.agent << step.tool;
    parts.removeAll(QString());
    return parts.join(QLatin1Char(' ')).toStdString();
}

std::string describe(const StreamEvent& event)
{
    switch (event.kind)
    {
        case StreamEvent::Kind::SessionStarted:
            return "session " + event.text.toStdString();
        case StreamEvent::Kind::StepStarted:
            return "start " + label(event.step);
        case StreamEvent::Kind::StepFinished:
            return "finish " + statusName(event.step.status) + " " + label(event.step);
        case StreamEvent::Kind::Answer:
            return "answer " + event.text.toStdString();
        case StreamEvent::Kind::Failure:
            return "failure " + event.text.toStdString();
    }
    return {};
}

std::vector<std::string> trace(const std::vector<StreamEvent>& events)
{
    std::vector<std::string> lines;
    std::transform(events.begin(), events.end(), std::back_inserter(lines), describe);
    return lines;
}

std::vector<StreamEvent> feedInChunks(StreamJsonParser& parser, const QByteArray& stream,
                                      qsizetype chunkSize)
{
    std::vector<StreamEvent> events;
    for (qsizetype at = 0; at < stream.size(); at += chunkSize)
    {
        std::vector<StreamEvent> more = parser.feed(stream.mid(at, chunkSize));
        std::move(more.begin(), more.end(), std::back_inserter(events));
    }
    return events;
}

std::vector<Step> stepsOf(const std::vector<StreamEvent>& events, StreamEvent::Kind kind)
{
    std::vector<Step> steps;
    for (const StreamEvent& event : events)
    {
        if (event.kind == kind)
            steps.push_back(event.step);
    }
    return steps;
}

QByteArray line(const QJsonObject& event)
{
    return QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n';
}

QJsonValue parentOf(const QString& toolUseId)
{
    return toolUseId.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(toolUseId);
}

QJsonObject initEvent(const QString& sessionId, const QJsonArray& servers = {})
{
    QJsonObject event { { "type", "system" }, { "subtype", "init" }, { "mcp_servers", servers } };
    if (!sessionId.isEmpty())
        event.insert("session_id", sessionId);
    return event;
}

QJsonObject toolUse(const QString& id, const QString& name, const QJsonObject& input = {})
{
    return { { "type", "tool_use" }, { "id", id }, { "name", name }, { "input", input } };
}

QJsonObject assistantEvent(const QJsonArray& blocks, const QString& parentToolUseId = {})
{
    return { { "type", "assistant" },
             { "message", QJsonObject { { "content", blocks } } },
             { "parent_tool_use_id", parentOf(parentToolUseId) } };
}

QJsonObject toolResultEvent(const QString& toolUseId, const QString& text, bool isError = false,
                            const QString& parentToolUseId = {})
{
    QJsonObject block { { "type", "tool_result" },
                        { "tool_use_id", toolUseId },
                        { "content", text } };
    if (isError)
        block.insert("is_error", true);
    return { { "type", "user" },
             { "message", QJsonObject { { "content", QJsonArray { block } } } },
             { "parent_tool_use_id", parentOf(parentToolUseId) } };
}

QJsonObject taskNotification(const QString& toolUseId, const QString& status,
                             const QString& summary)
{
    return { { "type", "system" },
             { "subtype", "task_notification" },
             { "tool_use_id", toolUseId },
             { "status", status },
             { "summary", summary } };
}

QJsonObject resultEvent(const QString& subtype, bool isError, const QString& text)
{
    return {
        { "type", "result" }, { "subtype", subtype }, { "is_error", isError }, { "result", text }
    };
}

const QString kAnswer
    = QStringLiteral("Your courses are:\n\n- Linux Kernel Basics\n- C++ Memory Model");

}

class StreamJsonParserTest : public ::testing::Test
{
protected:
    std::vector<StreamEvent> parse(const char* fixture)
    {
        const QByteArray stream = readFixture(fixture);
        return feedInChunks(parser, stream, stream.size());
    }

    std::vector<StreamEvent> parseStream(const QByteArray& stream)
    {
        return feedInChunks(parser, stream, stream.size());
    }

    StreamJsonParser parser;
};

TEST_F(StreamJsonParserTest, TurnsADelegatedTurnIntoStepsAndAnAnswer)
{
    const std::vector<StreamEvent> events = parse("sync-delegated-turn.jsonl");

    EXPECT_EQ(trace(events),
              (std::vector<std::string> {
                  "session 279f34e9-17bc-4a5d-a2b2-d0b5e6d014c2", "start delegation courses",
                  "start tool courses courses_list", "finish ok tool courses courses_list",
                  "finish ok delegation courses", "answer " + kAnswer.toStdString() }));
    EXPECT_TRUE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, PutsTheToolStepUnderItsDelegation)
{
    const std::vector<StreamEvent> events = parse("sync-delegated-turn.jsonl");
    const std::vector<Step> started = stepsOf(events, StreamEvent::Kind::StepStarted);
    const std::vector<Step> finished = stepsOf(events, StreamEvent::Kind::StepFinished);

    ASSERT_EQ(started.size(), 2u);
    ASSERT_EQ(finished.size(), 2u);
    EXPECT_EQ(started[0].kind, StepKind::Delegation);
    EXPECT_FALSE(started[0].parentId.has_value());
    EXPECT_EQ(started[1].kind, StepKind::Tool);
    EXPECT_EQ(started[1].parentId, started[0].id);
    EXPECT_NE(started[0].id, started[1].id);
    EXPECT_FALSE(started[0].id.isNull());
    EXPECT_EQ(finished[0].id, started[1].id);
    EXPECT_EQ(finished[1].id, started[0].id);
    EXPECT_EQ(started[0].status, StepStatus::Running);
}

TEST_F(StreamJsonParserTest, KeepsTheInputOfEveryStep)
{
    const std::vector<StreamEvent> events = parse("sync-delegated-turn.jsonl");
    const std::vector<Step> started = stepsOf(events, StreamEvent::Kind::StepStarted);

    ASSERT_EQ(started.size(), 2u);
    EXPECT_EQ(started[0].agent, "courses");
    EXPECT_EQ(started[0].input.value("subagent_type").toString(), "courses");
    EXPECT_EQ(started[0].input.value("description").toString(), "List user's courses");
    EXPECT_TRUE(started[0].input.value("prompt").toString().startsWith("The user asks:"));
    EXPECT_TRUE(started[0].tool.isEmpty());
    EXPECT_EQ(started[1].tool, "courses_list");
    EXPECT_EQ(started[1].agent, "courses");
    EXPECT_TRUE(started[1].input.isEmpty());
}

TEST_F(StreamJsonParserTest, ReportsTheToolResultTextAndTheDelegationSummaryAsOutput)
{
    const std::vector<StreamEvent> events = parse("sync-delegated-turn.jsonl");
    const std::vector<Step> finished = stepsOf(events, StreamEvent::Kind::StepFinished);

    ASSERT_EQ(finished.size(), 2u);
    EXPECT_EQ(finished[0].output.toString(),
              "[{\"id\": 1, \"title\": \"Linux Kernel Basics\", \"facts\": 42}, "
              "{\"id\": 2, \"title\": \"C++ Memory Model\", \"facts\": 17}]");
    EXPECT_EQ(finished[1].output.toString(), "Linux Kernel Basics\nC++ Memory Model");
}

TEST_F(StreamJsonParserTest, FailsTheToolStepOnAnErrorResultAndKeepsTheTurnGoing)
{
    const std::vector<StreamEvent> events = parse("tool-error.jsonl");

    const std::vector<std::string> lines = trace(events);
    ASSERT_EQ(lines.size(), 6u);
    EXPECT_EQ(lines[1], "start delegation fragile");
    EXPECT_EQ(lines[2], "start tool fragile courses_broken");
    EXPECT_EQ(lines[3], "finish failed tool fragile courses_broken");
    EXPECT_EQ(lines[4], "finish ok delegation fragile");
    EXPECT_TRUE(lines[5].starts_with("answer The fragile agent could not list your courses."));
    EXPECT_EQ(stepsOf(events, StreamEvent::Kind::StepFinished)[0].output.toString(),
              "Error: database is locked");
}

TEST_F(StreamJsonParserTest, FailsTheToolStepWhenThePermissionIsDenied)
{
    const std::vector<StreamEvent> events = parse("permission-denied.jsonl");

    const std::vector<std::string> lines = trace(events);
    ASSERT_EQ(lines.size(), 6u);
    EXPECT_EQ(lines[2], "start tool courses courses_list");
    EXPECT_EQ(lines[3], "finish failed tool courses courses_list");
    EXPECT_EQ(lines[4], "finish ok delegation courses");
    EXPECT_TRUE(lines[5].starts_with("answer I couldn't get your course list."));
    EXPECT_TRUE(stepsOf(events, StreamEvent::Kind::StepFinished)[0].output.toString().startsWith(
        "Permission to use mcp__app__courses_list has been denied"));
}

TEST_F(StreamJsonParserTest, FailsTheDelegationOnAFailedTaskNotification)
{
    const std::vector<StreamEvent> events = parse("mcp-403.jsonl");

    const std::vector<std::string> lines = trace(events);
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0], "session d8bdd5c1-e16d-43e1-9302-b8bb3c4cc778");
    EXPECT_EQ(lines[1], "start delegation courses");
    EXPECT_EQ(lines[2], "finish failed delegation courses");
    EXPECT_TRUE(lines[3].starts_with("answer I couldn't retrieve your courses."));
    EXPECT_TRUE(stepsOf(events, StreamEvent::Kind::StepFinished)[0].output.toString().startsWith(
        "Agent 'courses' would be spawned with zero tools"));
}

TEST_F(StreamJsonParserTest, ReadsTheAgentsListedByTheInitEvent)
{
    parse("sync-delegated-turn.jsonl");

    EXPECT_EQ(parser.agents(),
              (QStringList { "claude", "courses", "Explore", "fragile", "general-purpose", "Plan",
                             "statusline-setup" }));
}

TEST_F(StreamJsonParserTest, GivesTheSameEventsWhateverTheChunking)
{
    for (const char* fixture : { "sync-delegated-turn.jsonl", "tool-error.jsonl",
                                 "permission-denied.jsonl", "mcp-403.jsonl" })
    {
        const QByteArray stream = readFixture(fixture);
        StreamJsonParser whole;
        const std::vector<std::string> expected = trace(feedInChunks(whole, stream, stream.size()));

        for (qsizetype chunkSize : { 1, 3, 7, 64, 1000 })
        {
            StreamJsonParser chunked;
            EXPECT_EQ(trace(feedInChunks(chunked, stream, chunkSize)), expected)
                << fixture << " in chunks of " << chunkSize;
        }
    }
}

TEST_F(StreamJsonParserTest, WaitsForTheEndOfTheLineBeforeParsing)
{
    const QByteArray stream = readFixture("sync-delegated-turn.jsonl").trimmed();

    const std::vector<StreamEvent> events = parseStream(stream);

    ASSERT_FALSE(events.empty());
    EXPECT_EQ(describe(events.back()), "finish ok delegation courses");
    EXPECT_FALSE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, ParsesTheUnterminatedLastLineAtTheEndOfTheStream)
{
    parseStream(readFixture("sync-delegated-turn.jsonl").trimmed());

    const std::vector<StreamEvent> tail = parser.endOfStream();

    EXPECT_EQ(trace(tail), (std::vector<std::string> { "answer " + kAnswer.toStdString() }));
    EXPECT_TRUE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, FailsTheRunningStepsWhenTheStreamEndsFirst)
{
    const QList<QByteArray> lines = readFixture("sync-delegated-turn.jsonl").split('\n');
    const QByteArray stream = lines.mid(0, 7).join('\n') + '\n';
    parseStream(stream);

    const std::vector<StreamEvent> tail = parser.endOfStream();

    EXPECT_EQ(
        trace(tail),
        (std::vector<std::string> { "finish failed tool courses courses_list",
                                    "finish failed delegation courses", "failure server_error" }));
    EXPECT_TRUE(parser.isFinished());
    EXPECT_TRUE(parser.feed(readFixture("sync-delegated-turn.jsonl")).empty());
    EXPECT_TRUE(parser.endOfStream().empty());
}

TEST_F(StreamJsonParserTest, EndsWithAServerErrorWhenTheStreamHasNoResult)
{
    const std::vector<StreamEvent> events = parser.endOfStream();

    EXPECT_EQ(trace(events), (std::vector<std::string> { "failure server_error" }));
    EXPECT_EQ(events[0].detail, "stream ended without a result");
    EXPECT_TRUE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, ClosesTheRunningStepsBeforeTheAnswer)
{
    const QByteArray stream
        = line(assistantEvent({ toolUse("a", "Agent", { { "subagent_type", "courses" } }) }))
        + line(resultEvent("success", false, "done"));

    EXPECT_EQ(trace(parseStream(stream)),
              (std::vector<std::string> { "start delegation courses",
                                          "finish failed delegation courses", "answer done" }));
}

TEST_F(StreamJsonParserTest, ReportsAServerErrorForAFailedResult)
{
    const QByteArray stream = line(resultEvent("error_during_execution", true, "boom"));

    const std::vector<StreamEvent> events = parseStream(stream);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, StreamEvent::Kind::Failure);
    EXPECT_EQ(events[0].text, turn_error::kServerError);
    EXPECT_EQ(events[0].detail, "boom");
}

TEST_F(StreamJsonParserTest, ReportsAServerErrorForASuccessResultFlaggedAsError)
{
    const std::vector<StreamEvent> events
        = parseStream(line(resultEvent("success", true, "rate limited")));

    EXPECT_EQ(trace(events), (std::vector<std::string> { "failure server_error" }));
    EXPECT_EQ(events[0].detail, "rate limited");
}

TEST_F(StreamJsonParserTest, IgnoresLinesItDoesNotUnderstand)
{
    const QByteArray stream = QByteArray("not json at all\n") + "\n" + "[1,2,3]\n"
        + "{\"no_type\":true}\n" + line({ { "type", "brand_new_event" } })
        + line({ { "type", "system" }, { "subtype", "future_subtype" } })
        + line({ { "type", "system" }, { "subtype", "dev_intent" } })
        + line({ { "type", "system" }, { "subtype", "task_updated" } })
        + line(assistantEvent({ QJsonObject { { "type", "thinking" } },
                                QJsonObject { { "type", "text" }, { "text", "thinking aloud" } } }))
        + line({ { "type", "user" }, { "message", QJsonObject() } })
        + line({ { "type", "rate_limit_event" } }) + line(resultEvent("success", false, "fine"));

    EXPECT_EQ(trace(parseStream(stream)), (std::vector<std::string> { "answer fine" }));
}

TEST_F(StreamJsonParserTest, IgnoresEverythingAfterTheFirstResult)
{
    const QByteArray stream = line(resultEvent("success", false, "first"))
        + line(resultEvent("success", false, "second"))
        + line(assistantEvent({ toolUse("a", "Agent") }));

    EXPECT_EQ(trace(parseStream(stream)), (std::vector<std::string> { "answer first" }));
    EXPECT_TRUE(parser.feed(line(resultEvent("success", false, "third"))).empty());
}

TEST_F(StreamJsonParserTest, ReportsTheSessionOnlyOnce)
{
    const QByteArray stream = line(initEvent("session-1")) + line(initEvent("session-2"));

    EXPECT_EQ(trace(parseStream(stream)), (std::vector<std::string> { "session session-1" }));
}

TEST_F(StreamJsonParserTest, ReportsNoSessionWhenTheInitEventHasNone)
{
    EXPECT_TRUE(parseStream(line(initEvent(""))).empty());
}

TEST_F(StreamJsonParserTest, DoesNotEndTheTurnForAFailedServerListedByTheInitEvent)
{
    const QJsonArray servers { QJsonObject { { "name", "app" }, { "status", "failed" } },
                               QJsonObject { { "name", "other" }, { "status", "connected" } } };

    EXPECT_EQ(trace(parseStream(line(initEvent("s1", servers)))),
              (std::vector<std::string> { "session s1" }));
    EXPECT_FALSE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, StartsEveryToolUseOfOneMessageAndFinishesThemInAnyOrder)
{
    const QByteArray stream
        = line(assistantEvent({ toolUse("a", "Agent", { { "subagent_type", "courses" } }),
                                toolUse("b", "Agent", { { "subagent_type", "lessons" } }) }))
        + line(toolResultEvent("b", "lesson text"))
        + line(toolResultEvent("a", "no courses", true));

    EXPECT_EQ(trace(parseStream(stream)),
              (std::vector<std::string> { "start delegation courses", "start delegation lessons",
                                          "finish ok delegation lessons",
                                          "finish failed delegation courses" }));
}

TEST_F(StreamJsonParserTest, IgnoresARepeatedToolUse)
{
    const QByteArray message
        = line(assistantEvent({ toolUse("a", "Agent", { { "subagent_type", "courses" } }) }));

    EXPECT_EQ(trace(parseStream(message + message)),
              (std::vector<std::string> { "start delegation courses" }));
}

TEST_F(StreamJsonParserTest, FinishesAStepOnlyOnce)
{
    const QByteArray stream
        = line(assistantEvent({ toolUse("a", "Agent", { { "subagent_type", "courses" } }) }))
        + line(taskNotification("a", "failed", "could not start"))
        + line(toolResultEvent("a", "fine now"));

    const std::vector<StreamEvent> events = parseStream(stream);

    EXPECT_EQ(trace(events),
              (std::vector<std::string> { "start delegation courses",
                                          "finish failed delegation courses" }));
    EXPECT_EQ(events[1].step.output.toString(), "could not start");
}

TEST_F(StreamJsonParserTest, IgnoresATaskNotificationInANonTerminalStatus)
{
    const QByteArray stream
        = line(assistantEvent({ toolUse("a", "Agent", { { "subagent_type", "courses" } }) }))
        + line(taskNotification("a", "running", "still going"))
        + line(taskNotification("unknown", "failed", "nobody asked"))
        + line(toolResultEvent("a", "done"));

    EXPECT_EQ(
        trace(parseStream(stream)),
        (std::vector<std::string> { "start delegation courses", "finish ok delegation courses" }));
}

TEST_F(StreamJsonParserTest, IgnoresAToolResultForAnUnknownToolUse)
{
    EXPECT_TRUE(parseStream(line(toolResultEvent("ghost", "text"))).empty());
}

TEST_F(StreamJsonParserTest, KeepsAToolWithoutAKnownParentAtTheTop)
{
    const QByteArray stream
        = line(assistantEvent({ toolUse("t", "mcp__app__courses_list") }, "ghost"));

    const std::vector<Step> started = stepsOf(parseStream(stream), StreamEvent::Kind::StepStarted);

    ASSERT_EQ(started.size(), 1u);
    EXPECT_FALSE(started[0].parentId.has_value());
    EXPECT_TRUE(started[0].agent.isEmpty());
    EXPECT_EQ(started[0].tool, "courses_list");
}

TEST_F(StreamJsonParserTest, KeepsTheNameOfAToolOutsideTheAppServer)
{
    const QByteArray stream = line(assistantEvent({ toolUse("t", "Bash") }));

    const std::vector<Step> started = stepsOf(parseStream(stream), StreamEvent::Kind::StepStarted);

    ASSERT_EQ(started.size(), 1u);
    EXPECT_EQ(started[0].tool, "Bash");
}

TEST_F(StreamJsonParserTest, JoinsTheTextBlocksOfAToolResult)
{
    const QJsonObject result {
        { "type", "user" },
        { "message",
          QJsonObject { { "content",
                          QJsonArray { QJsonObject {
                              { "type", "tool_result" },
                              { "tool_use_id", "t" },
                              { "content",
                                QJsonArray { QJsonObject { { "type", "text" }, { "text", "one" } },
                                             QJsonObject { { "type", "image" } },
                                             QJsonObject { { "type", "text" },
                                                           { "text", "two" } } } } } } } } }
    };
    const QByteArray stream = line(assistantEvent({ toolUse("t", "mcp__app__x") })) + line(result);

    const std::vector<Step> finished
        = stepsOf(parseStream(stream), StreamEvent::Kind::StepFinished);

    ASSERT_EQ(finished.size(), 1u);
    EXPECT_EQ(finished[0].output.toString(), "one\ntwo");
}

TEST_F(StreamJsonParserTest, GivesEveryStepItsOwnId)
{
    const std::vector<StreamEvent> events = parse("sync-delegated-turn.jsonl");

    std::set<QUuid> ids;
    for (const Step& step : stepsOf(events, StreamEvent::Kind::StepStarted))
        ids.insert(step.id);
    EXPECT_EQ(ids.size(), 2u);
}
