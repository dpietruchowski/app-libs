#include <gtest/gtest.h>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "agent/streamjsonwriter.h"
#include "fixturefile.h"

namespace
{

QJsonObject parsedLine(const QByteArray& line)
{
    EXPECT_TRUE(line.endsWith('\n'));
    EXPECT_EQ(line.count('\n'), 1);
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line.trimmed(), &error);
    EXPECT_EQ(error.error, QJsonParseError::NoError) << error.errorString().toStdString();
    return document.object();
}

}

TEST(StreamJsonWriterTest, WritesTheInitializeRequest)
{
    const QJsonObject expected { { "type", "control_request" },
                                 { "request_id", "init_1" },
                                 { "request",
                                   QJsonObject { { "subtype", "initialize" },
                                                 { "hooks", QJsonValue(QJsonValue::Null) } } } };

    EXPECT_EQ(parsedLine(StreamJsonWriter::initialize()), expected);
}

TEST(StreamJsonWriterTest, WritesAUserMessage)
{
    const QJsonObject expected { { "type", "user" },
                                 { "message",
                                   QJsonObject { { "role", "user" }, { "content", "Hello" } } },
                                 { "parent_tool_use_id", QJsonValue(QJsonValue::Null) },
                                 { "session_id", "default" } };

    EXPECT_EQ(parsedLine(StreamJsonWriter::userMessage("Hello")), expected);
}

TEST(StreamJsonWriterTest, KeepsTheTextOfAUserMessageWhole)
{
    const QString text = QString::fromUtf8("[user] \"Zażółć\" gęślą\n[assistant] jaźń\t\\ end");

    const QJsonObject line = parsedLine(StreamJsonWriter::userMessage(text));

    EXPECT_EQ(line.value("message").toObject().value("content").toString(), text);
}

TEST(StreamJsonWriterTest, KeepsEveryLineOnOneLine)
{
    const QByteArray line = StreamJsonWriter::userMessage("first\nsecond\r\nthird");

    EXPECT_EQ(line.count('\n'), 1);
    EXPECT_TRUE(line.endsWith('\n'));
}

TEST(StreamJsonWriterTest, WrapsAnMcpResponseInAControlResponse)
{
    const QJsonObject rpc { { "jsonrpc", "2.0" },
                            { "id", 3 },
                            { "result", QJsonObject { { "tools", QJsonArray() } } } };
    const QJsonObject expected {
        { "type", "control_response" },
        { "response",
          QJsonObject { { "subtype", "success" },
                        { "request_id", "req-42" },
                        { "response", QJsonObject { { "mcp_response", rpc } } } } }
    };

    EXPECT_EQ(parsedLine(StreamJsonWriter::mcpResponse("req-42", rpc)), expected);
}

TEST(StreamJsonWriterTest, ReproducesTheLinesTheHostWroteInARecordedRun)
{
    const QList<QByteArray> written = readFixtureSide("stdio-tool-run.jsonl", kHostSays);
    ASSERT_EQ(written.size(), 6);

    EXPECT_EQ(parsedLine(StreamJsonWriter::initialize()),
              QJsonDocument::fromJson(written[0]).object());
    EXPECT_EQ(parsedLine(StreamJsonWriter::userMessage("List my courses.")),
              QJsonDocument::fromJson(written[1]).object());
    for (qsizetype at = 2; at < written.size(); ++at)
    {
        const QJsonObject recorded = QJsonDocument::fromJson(written[at]).object();
        const QJsonObject response = recorded.value("response").toObject();
        const QByteArray rebuilt = StreamJsonWriter::mcpResponse(
            response.value("request_id").toString(),
            response.value("response").toObject().value("mcp_response").toObject());
        EXPECT_EQ(parsedLine(rebuilt), recorded) << "host line " << at;
    }
}

TEST(StreamJsonWriterTest, KeepsTheResponseOfAFailedRpcAsItIs)
{
    const QJsonObject rpc {
        { "jsonrpc", "2.0" },
        { "id", QJsonValue(QJsonValue::Null) },
        { "error", QJsonObject { { "code", -32601 }, { "message", "Method not found" } } }
    };

    const QJsonObject response = parsedLine(StreamJsonWriter::mcpResponse("req-1", rpc))
                                     .value("response")
                                     .toObject()
                                     .value("response")
                                     .toObject();

    EXPECT_EQ(response.value("mcp_response").toObject(), rpc);
}
