#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "agent/claudecli.h"
#include "agent/streamjsonparser.h"
#include "fixturefile.h"

namespace
{

std::string describe(const StreamEvent& event)
{
    switch (event.kind)
    {
        case StreamEvent::Kind::SessionStarted:
            return "session " + event.text.toStdString();
        case StreamEvent::Kind::PartialText:
            return "partial " + event.text.toStdString();
        case StreamEvent::Kind::McpRequest:
            return "mcp " + event.serverName.toStdString() + " "
                + event.message.toObject().value("method").toString().toStdString();
        case StreamEvent::Kind::Answer:
            return "answer " + event.text.toStdString();
        case StreamEvent::Kind::Failure:
            return "failure " + event.text.toStdString();
        case StreamEvent::Kind::SessionMissing:
            return "session missing";
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

QByteArray line(const QJsonObject& event)
{
    return QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n';
}

QJsonObject initEvent(const QString& sessionId, const QJsonArray& servers = {})
{
    QJsonObject event { { "type", "system" }, { "subtype", "init" }, { "mcp_servers", servers } };
    if (!sessionId.isEmpty())
        event.insert("session_id", sessionId);
    return event;
}

QJsonObject resultEvent(const QString& subtype, bool isError, const QString& text)
{
    return {
        { "type", "result" }, { "subtype", subtype }, { "is_error", isError }, { "result", text }
    };
}

QJsonObject assistantErrorEvent(const QString& error)
{
    return { { "type", "assistant" },
             { "error", error },
             { "message", QJsonObject { { "role", "assistant" } } } };
}

QJsonObject errorsEvent(const QString& subtype, const QStringList& errors)
{
    return { { "type", "result" },
             { "subtype", subtype },
             { "is_error", true },
             { "errors", QJsonArray::fromStringList(errors) } };
}

QJsonObject mcpRequestEvent(const QString& requestId, const QString& serverName,
                            const QJsonValue& message, const QString& subtype = "mcp_message")
{
    return { { "type", "control_request" },
             { "request_id", requestId },
             { "request",
               QJsonObject { { "subtype", subtype },
                             { "server_name", serverName },
                             { "message", message } } } };
}

QJsonObject streamEvent(const QJsonObject& inner)
{
    return { { "type", "stream_event" },
             { "event", inner },
             { "parent_tool_use_id", QJsonValue(QJsonValue::Null) },
             { "session_id", "s1" } };
}

QJsonObject messageStart() { return streamEvent({ { "type", "message_start" } }); }

QJsonObject messageStartWithUsage(int fresh, int written, int read)
{
    const QJsonObject usage { { "input_tokens", fresh },
                              { "cache_creation_input_tokens", written },
                              { "cache_read_input_tokens", read },
                              { "output_tokens", 1 } };
    return streamEvent(
        { { "type", "message_start" }, { "message", QJsonObject { { "usage", usage } } } });
}

QJsonObject resultEventWithUsage(int fresh, int written, int read, int output)
{
    QJsonObject event = resultEvent("success", false, "fine");
    event.insert("usage",
                 QJsonObject { { "input_tokens", fresh },
                               { "cache_creation_input_tokens", written },
                               { "cache_read_input_tokens", read },
                               { "output_tokens", output } });
    return event;
}

QJsonObject deltaOf(const QString& deltaType, const QString& field, const QString& value)
{
    return streamEvent({ { "type", "content_block_delta" },
                         { "index", 0 },
                         { "delta", QJsonObject { { "type", deltaType }, { field, value } } } });
}

QJsonObject textDelta(const QString& text) { return deltaOf("text_delta", "text", text); }

std::vector<std::string> partialTexts(const std::vector<StreamEvent>& events)
{
    std::vector<std::string> texts;
    for (const StreamEvent& event : events)
    {
        if (event.kind == StreamEvent::Kind::PartialText)
            texts.push_back(event.text.toStdString());
    }
    return texts;
}

const QString kSessionId = QStringLiteral("64174da0-2cdb-4392-b7ec-5fe0b2f28b20");
const QString kAnswer
    = QStringLiteral("Your courses:\n\n- Linux Kernel Basics\n- C++ Memory Model");

}

class StreamJsonParserTest : public ::testing::Test
{
protected:
    std::vector<StreamEvent> parseFixture(const char* fixture)
    {
        return parseStream(readFixtureStream(fixture, kCliSays));
    }

    std::vector<StreamEvent> parseStream(const QByteArray& stream)
    {
        return feedInChunks(parser, stream, stream.size());
    }

    StreamJsonParser parser;
};

TEST_F(StreamJsonParserTest, TurnsARunWithAToolCallIntoRequestsAndAnAnswer)
{
    const std::vector<StreamEvent> events = parseFixture("stdio-tool-run.jsonl");

    EXPECT_EQ(
        trace(events),
        (std::vector<std::string> { "mcp app initialize", "mcp app notifications/initialized",
                                    "mcp app tools/list", "session " + kSessionId.toStdString(),
                                    "mcp app tools/call", "answer " + kAnswer.toStdString() }));
    EXPECT_TRUE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, KeepsTheRequestIdServerNameAndMessageOfAnMcpRequest)
{
    const std::vector<StreamEvent> events = parseFixture("stdio-tool-run.jsonl");

    ASSERT_EQ(events.size(), 6u);
    EXPECT_EQ(events[2].requestId, "12bf375a-cae3-44dd-9613-9514ac6b8537");
    EXPECT_EQ(events[2].serverName, "app");
    EXPECT_EQ(events[2].message.toObject().value("method").toString(), "tools/list");
    EXPECT_EQ(events[2].message.toObject().value("id").toInt(), 1);

    const QJsonObject call = events[4].message.toObject();
    EXPECT_EQ(call.value("id").toInt(), 2);
    EXPECT_EQ(call.value("params").toObject().value("name").toString(), "courses_list");
    EXPECT_TRUE(call.value("params").toObject().value("arguments").toObject().isEmpty());
}

TEST_F(StreamJsonParserTest, CountsEveryKindOfInputTokenOfTheRunInThePromptTokensOfTheAnswer)
{
    const std::vector<StreamEvent> events = parseFixture("stdio-tool-run.jsonl");

    ASSERT_FALSE(events.empty());
    ASSERT_EQ(events.back().kind, StreamEvent::Kind::Answer);
    EXPECT_EQ(events.back().usage.prompt_tokens, 4 + 1114 + 1033);
    EXPECT_EQ(events.back().usage.completion_tokens, 62);
    EXPECT_EQ(events.back().usage.total_tokens, 4 + 1114 + 1033 + 62);
}

TEST_F(StreamJsonParserTest, ReportsNoTokensForAnAnswerWithoutUsage)
{
    const std::vector<StreamEvent> events
        = parseStream(line(resultEvent("success", false, "fine")));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].usage.prompt_tokens, 0);
    EXPECT_EQ(events[0].usage.completion_tokens, 0);
    EXPECT_EQ(events[0].usage.total_tokens, 0);
}

TEST_F(StreamJsonParserTest, ReportsTheInputOfTheLastRequestOfTheRunAsTheContextTokensOfTheAnswer)
{
    const QByteArray stream = line(messageStartWithUsage(3, 100, 2000))
        + line(messageStartWithUsage(5, 200, 4000)) + line(resultEventWithUsage(8, 300, 6000, 9));

    const std::vector<StreamEvent> events = parseStream(stream);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].usage.context_tokens, 5 + 200 + 4000);
    EXPECT_EQ(events[0].usage.prompt_tokens, 8 + 300 + 6000);
}

TEST_F(StreamJsonParserTest, ReportsNoContextTokensWithoutAMessageStartWithUsage)
{
    const QByteArray stream = line(messageStart()) + line(resultEventWithUsage(8, 300, 6000, 9));

    const std::vector<StreamEvent> events = parseStream(stream);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].usage.context_tokens, 0);
    EXPECT_EQ(events[0].usage.prompt_tokens, 8 + 300 + 6000);
}

TEST_F(StreamJsonParserTest, GivesTheSameEventsWhateverTheChunking)
{
    for (const char* fixture :
         { "stdio-tool-run.jsonl", "stdio-missing-session.jsonl", "stdio-partial-run.jsonl" })
    {
        const QByteArray stream = readFixtureStream(fixture, kCliSays);
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

TEST_F(StreamJsonParserTest, ReportsAMissingSessionWhenTheResultComesBeforeAnyInit)
{
    const std::vector<StreamEvent> events = parseFixture("stdio-missing-session.jsonl");

    EXPECT_EQ(trace(events), (std::vector<std::string> { "session missing" }));
    EXPECT_EQ(events[0].detail,
              "No conversation found with session ID: "
              "11111111-2222-4333-8444-555555555555");
    EXPECT_TRUE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, ReportsAServerErrorForAnExecutionErrorAfterTheInit)
{
    const QByteArray stream
        = line(initEvent("s1")) + line(resultEvent("error_during_execution", true, "boom"));

    const std::vector<StreamEvent> events = parseStream(stream);

    EXPECT_EQ(trace(events), (std::vector<std::string> { "session s1", "failure server_error" }));
    EXPECT_EQ(events[1].text, claudecli::kServerError);
    EXPECT_EQ(events[1].detail, "boom");
}

TEST_F(StreamJsonParserTest, ReportsAServerErrorForAnotherErrorBeforeTheInit)
{
    const std::vector<StreamEvent> events
        = parseStream(line(resultEvent("error_max_turns", true, "too long")));

    EXPECT_EQ(trace(events), (std::vector<std::string> { "failure server_error" }));
    EXPECT_EQ(events[0].detail, "too long");
}

TEST_F(StreamJsonParserTest, JoinsTheErrorsOfAFailedResultWithoutText)
{
    const std::vector<StreamEvent> events
        = parseStream(line(errorsEvent("error_max_turns", { "first", "second" })));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, StreamEvent::Kind::Failure);
    EXPECT_EQ(events[0].detail, "first\nsecond");
}

TEST_F(StreamJsonParserTest, PrefersTheResultTextToTheErrorsAsTheDetail)
{
    QJsonObject event = errorsEvent("error_max_turns", { "ignored" });
    event.insert("result", "the text");

    const std::vector<StreamEvent> events = parseStream(line(event));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].detail, "the text");
}

TEST_F(StreamJsonParserTest, ReportsAServerErrorForASuccessResultFlaggedAsError)
{
    const std::vector<StreamEvent> events
        = parseStream(line(resultEvent("success", true, "rate limited")));

    EXPECT_EQ(trace(events), (std::vector<std::string> { "failure server_error" }));
    EXPECT_EQ(events[0].detail, "rate limited");
}

TEST_F(StreamJsonParserTest, ReportsNotLoggedInForTheRecordedRunWithoutCredentials)
{
    const std::vector<StreamEvent> events = parseFixture("stdio-not-logged-in.jsonl");

    ASSERT_FALSE(events.empty());
    EXPECT_EQ(trace(events).back(), "failure not_logged_in");
    EXPECT_EQ(events.back().text, claudecli::kNotLoggedIn);
    EXPECT_EQ(events.back().detail, "Not logged in · Please run /login");
    EXPECT_TRUE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, ReportsNotLoggedInForAnErrorResultAfterAnAuthenticationFailedMessage)
{
    const std::vector<StreamEvent> events
        = parseStream(line(initEvent("s1")) + line(assistantErrorEvent("authentication_failed"))
                      + line(resultEvent("success", true, "The login has expired")));

    EXPECT_EQ(trace(events), (std::vector<std::string> { "session s1", "failure not_logged_in" }));
    EXPECT_EQ(events.back().detail, "The login has expired");
}

TEST_F(StreamJsonParserTest,
       ReportsAServerErrorForAnErrorResultWithoutAnAuthenticationFailedMessage)
{
    const std::vector<StreamEvent> events
        = parseStream(line(initEvent("s1"))
                      + line(resultEvent("success", true, "Not logged in · Please run /login")));

    EXPECT_EQ(trace(events), (std::vector<std::string> { "session s1", "failure server_error" }));
}

TEST_F(StreamJsonParserTest, ReportsAServerErrorWhenTheAssistantMessageCarriesAnotherError)
{
    const std::vector<StreamEvent> events
        = parseStream(line(assistantErrorEvent("rate_limit"))
                      + line(resultEvent("success", true, "Not logged in")));

    EXPECT_EQ(trace(events), (std::vector<std::string> { "failure server_error" }));
}

TEST_F(StreamJsonParserTest,
       ReportsAnAnswerWhenTheResultIsNoErrorAfterAnAuthenticationFailedMessage)
{
    const std::vector<StreamEvent> events
        = parseStream(line(assistantErrorEvent("authentication_failed"))
                      + line(resultEvent("success", false, "fine")));

    EXPECT_EQ(trace(events), (std::vector<std::string> { "answer fine" }));
}

TEST_F(StreamJsonParserTest, WaitsForTheEndOfTheLineBeforeParsing)
{
    const QByteArray stream = readFixtureStream("stdio-tool-run.jsonl", kCliSays).trimmed();

    const std::vector<StreamEvent> events = parseStream(stream);

    ASSERT_FALSE(events.empty());
    EXPECT_EQ(describe(events.back()), "mcp app tools/call");
    EXPECT_FALSE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, ParsesTheUnterminatedLastLineAtTheEndOfTheStream)
{
    parseStream(readFixtureStream("stdio-tool-run.jsonl", kCliSays).trimmed());

    const std::vector<StreamEvent> tail = parser.endOfStream();

    EXPECT_EQ(trace(tail), (std::vector<std::string> { "answer " + kAnswer.toStdString() }));
    EXPECT_TRUE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, EndsWithAServerErrorWhenTheStreamEndsBeforeTheResult)
{
    const QList<QByteArray> lines = readFixtureSide("stdio-tool-run.jsonl", kCliSays);
    parseStream(lines.mid(0, 5).join('\n') + '\n');

    const std::vector<StreamEvent> tail = parser.endOfStream();

    EXPECT_EQ(trace(tail), (std::vector<std::string> { "failure server_error" }));
    EXPECT_EQ(tail[0].detail, "stream ended without a result");
    EXPECT_TRUE(parser.isFinished());
    EXPECT_TRUE(parser.feed(readFixtureStream("stdio-tool-run.jsonl", kCliSays)).empty());
    EXPECT_TRUE(parser.endOfStream().empty());
}

TEST_F(StreamJsonParserTest, EndsWithAServerErrorWhenTheStreamIsEmpty)
{
    const std::vector<StreamEvent> events = parser.endOfStream();

    EXPECT_EQ(trace(events), (std::vector<std::string> { "failure server_error" }));
    EXPECT_EQ(events[0].detail, "stream ended without a result");
    EXPECT_TRUE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, IgnoresLinesItDoesNotUnderstand)
{
    const QByteArray stream = QByteArray("not json at all\n") + "\n" + "[1,2,3]\n"
        + "{\"no_type\":true}\n" + line({ { "type", "brand_new_event" } })
        + line({ { "type", "system" }, { "subtype", "future_subtype" } })
        + line({ { "type", "system" }, { "subtype", "task_notification" } })
        + line({ { "type", "assistant" }, { "message", QJsonObject() } })
        + line({ { "type", "user" }, { "message", QJsonObject() } })
        + line({ { "type", "tool_progress" } }) + line({ { "type", "rate_limit_event" } })
        + line({ { "type", "control_response" },
                 { "response",
                   QJsonObject { { "subtype", "success" }, { "request_id", "init_1" } } } })
        + line(resultEvent("success", false, "fine"));

    EXPECT_EQ(trace(parseStream(stream)), (std::vector<std::string> { "answer fine" }));
}

TEST_F(StreamJsonParserTest, IgnoresEverythingAfterTheFirstResult)
{
    const QByteArray stream = line(resultEvent("success", false, "first"))
        + line(resultEvent("success", false, "second"))
        + line(mcpRequestEvent("r1", "app", QJsonObject()));

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

TEST_F(StreamJsonParserTest, DoesNotEndTheRunForAFailedServerListedByTheInitEvent)
{
    const QJsonArray servers { QJsonObject { { "name", "app" }, { "status", "failed" } },
                               QJsonObject { { "name", "other" }, { "status", "connected" } } };

    EXPECT_EQ(trace(parseStream(line(initEvent("s1", servers)))),
              (std::vector<std::string> { "session s1" }));
    EXPECT_FALSE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, ReportsEveryMcpRequestInOrder)
{
    const QByteArray stream
        = line(mcpRequestEvent("r1", "app", QJsonObject { { "method", "tools/call" } }))
        + line(mcpRequestEvent("r2", "app", QJsonObject { { "method", "ping" } }));

    const std::vector<StreamEvent> events = parseStream(stream);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].requestId, "r1");
    EXPECT_EQ(events[1].requestId, "r2");
    EXPECT_EQ(trace(events), (std::vector<std::string> { "mcp app tools/call", "mcp app ping" }));
}

TEST_F(StreamJsonParserTest, PassesAMessageThatIsNotAnObjectThrough)
{
    const std::vector<StreamEvent> events
        = parseStream(line(mcpRequestEvent("r1", "app", QJsonArray { 1, 2 })));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, StreamEvent::Kind::McpRequest);
    EXPECT_TRUE(events[0].message.isArray());
}

TEST_F(StreamJsonParserTest, IgnoresAControlRequestOfAnotherSubtype)
{
    const QByteArray stream = line(mcpRequestEvent("r1", "app", QJsonObject(), "can_use_tool"));

    EXPECT_TRUE(parseStream(stream).empty());
    EXPECT_FALSE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, IgnoresAnMcpRequestWithoutARequestId)
{
    EXPECT_TRUE(parseStream(line(mcpRequestEvent("", "app", QJsonObject()))).empty());
}

TEST_F(StreamJsonParserTest, GrowsThePartialTextWithEachTextDelta)
{
    const QByteArray stream = line(messageStart()) + line(textDelta("Hel")) + line(textDelta("lo"))
        + line(textDelta(", world"));

    const std::vector<StreamEvent> events = parseStream(stream);

    EXPECT_EQ(partialTexts(events), (std::vector<std::string> { "Hel", "Hello", "Hello, world" }));
    EXPECT_FALSE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, RestartsThePartialTextWithEachMessage)
{
    const QByteArray stream = line(messageStart()) + line(textDelta("first")) + line(messageStart())
        + line(textDelta("second"));

    EXPECT_EQ(partialTexts(parseStream(stream)), (std::vector<std::string> { "first", "second" }));
}

TEST_F(StreamJsonParserTest, KeepsThePartialTextAcrossContentBlocksOfOneMessage)
{
    const QByteArray stream = line(messageStart()) + line(textDelta("one"))
        + line(streamEvent({ { "type", "content_block_stop" } }))
        + line(streamEvent({ { "type", "content_block_start" } })) + line(textDelta(" two"));

    EXPECT_EQ(partialTexts(parseStream(stream)), (std::vector<std::string> { "one", "one two" }));
}

TEST_F(StreamJsonParserTest, IgnoresStreamEventsThatCarryNoText)
{
    const QByteArray stream = line(messageStart())
        + line(streamEvent({ { "type", "content_block_start" } }))
        + line(deltaOf("thinking_delta", "thinking", "pondering"))
        + line(deltaOf("signature_delta", "signature", "abc"))
        + line(deltaOf("input_json_delta", "partial_json", "{\"a\":1}"))
        + line(streamEvent({ { "type", "content_block_stop" } }))
        + line(streamEvent({ { "type", "message_delta" } }))
        + line(streamEvent({ { "type", "message_stop" } })) + line({ { "type", "stream_event" } })
        + line({ { "type", "stream_event" }, { "event", "not an object" } });

    EXPECT_TRUE(parseStream(stream).empty());
    EXPECT_FALSE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, IgnoresATextDeltaOutsideAContentBlockDelta)
{
    const QJsonObject misplaced = streamEvent(
        { { "type", "message_delta" },
          { "delta", QJsonObject { { "type", "text_delta" }, { "text", "stray" } } } });

    EXPECT_TRUE(parseStream(line(misplaced)).empty());
}

TEST_F(StreamJsonParserTest, StartsThePartialTextAtTheFirstDeltaWithoutAMessageStart)
{
    EXPECT_EQ(partialTexts(parseStream(line(textDelta("abc")))),
              (std::vector<std::string> { "abc" }));
}

TEST_F(StreamJsonParserTest, StreamsTheTextOfAMessageBeforeAndAfterAToolCall)
{
    const std::vector<StreamEvent> events = parseFixture("stdio-partial-run.jsonl");
    const std::vector<std::string> lines = trace(events);

    const auto firstPartial = std::find(lines.begin(), lines.end(), "partial Checking your");
    const auto toolCall = std::find(lines.begin(), lines.end(), "mcp app tools/call");
    const auto secondMessage = std::find(lines.begin(), lines.end(), "partial You");
    ASSERT_NE(firstPartial, lines.end());
    ASSERT_NE(toolCall, lines.end());
    ASSERT_NE(secondMessage, lines.end());
    EXPECT_LT(firstPartial, toolCall);
    EXPECT_LT(toolCall, secondMessage);
    EXPECT_EQ(*(firstPartial + 1), "partial Checking your courses now.");
    EXPECT_EQ(*(secondMessage + 1), "partial You have two courses:\n\n1");
}

TEST_F(StreamJsonParserTest, EndsTheRecordedStreamWithThePartialTextEqualToTheAnswer)
{
    const std::vector<StreamEvent> events = parseFixture("stdio-partial-run.jsonl");

    ASSERT_GE(events.size(), 2u);
    const StreamEvent& answer = events.back();
    ASSERT_EQ(answer.kind, StreamEvent::Kind::Answer);
    const std::vector<std::string> texts = partialTexts(events);
    ASSERT_FALSE(texts.empty());
    EXPECT_EQ(texts.back(), answer.text.toStdString());
    EXPECT_TRUE(parser.isFinished());
}

TEST_F(StreamJsonParserTest, EmitsOnePartialTextForEveryTextDeltaOfARecordedStream)
{
    const QByteArray stream = readFixtureStream("stdio-partial-run.jsonl", kCliSays);

    const std::vector<StreamEvent> events = parseStream(stream);

    EXPECT_EQ(partialTexts(events).size(),
              static_cast<size_t>(stream.count(QByteArray("\"text_delta\""))));
}
