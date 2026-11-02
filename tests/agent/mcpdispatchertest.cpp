#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>

#include "agent/mcpdispatcher.h"
#include "fixturefile.h"
#include "toolsmapof.h"

namespace
{

const QString kRequestId = QStringLiteral("req-1");

QJsonObject rpc(const QString& method, const QJsonValue& id, const QJsonObject& params = {})
{
    QJsonObject message { { "jsonrpc", "2.0" }, { "method", method } };
    if (!id.isUndefined())
        message.insert("id", id);
    if (!params.isEmpty())
        message.insert("params", params);
    return message;
}

QJsonObject callParams(const QString& name, const QJsonObject& arguments = {})
{
    QJsonObject params { { "name", name } };
    if (!arguments.isEmpty())
        params.insert("arguments", arguments);
    return params;
}

QList<QJsonObject> recordedRequests(const char* fixture)
{
    QList<QJsonObject> requests;
    for (const QByteArray& line : readFixtureSide(fixture, kCliSays))
    {
        const QJsonObject event = QJsonDocument::fromJson(line).object();
        if (event.value("type").toString() == "control_request"
            && event.value("request").toObject().value("subtype").toString() == "mcp_message")
            requests.append(event);
    }
    return requests;
}

QList<QJsonObject> recordedReplies(const char* fixture)
{
    QList<QJsonObject> replies;
    for (const QByteArray& line : readFixtureSide(fixture, kHostSays))
    {
        const QJsonObject event = QJsonDocument::fromJson(line).object();
        if (event.value("type").toString() == "control_response")
        {
            replies.append(event.value("response")
                               .toObject()
                               .value("response")
                               .toObject()
                               .value("mcp_response")
                               .toObject());
        }
    }
    return replies;
}

QJsonObject errorOf(const McpAction& action) { return action.reply.value("error").toObject(); }

QJsonObject resultOf(const McpAction& action) { return action.reply.value("result").toObject(); }

}

TEST(McpDispatcherTest, AnswersInitializeWithTheRequestedProtocolVersion)
{
    const McpDispatcher dispatcher(ToolsMap {});

    const McpAction action = dispatcher.handle(
        kRequestId, rpc("initialize", 0, { { "protocolVersion", "2024-11-05" } }));

    EXPECT_EQ(action.kind, McpAction::Kind::Reply);
    EXPECT_EQ(action.reply.value("jsonrpc").toString(), "2.0");
    EXPECT_EQ(action.reply.value("id").toInt(-1), 0);
    EXPECT_EQ(resultOf(action).value("protocolVersion").toString(), "2024-11-05");
    EXPECT_EQ(resultOf(action).value("capabilities").toObject(),
              (QJsonObject { { "tools", QJsonObject() } }));
    EXPECT_EQ(resultOf(action).value("serverInfo").toObject(),
              (QJsonObject { { "name", "app" }, { "version", "1" } }));
}

TEST(McpDispatcherTest, AnswersInitializeWithTheDefaultVersionWhenNoneIsRequested)
{
    const McpDispatcher dispatcher(ToolsMap {});

    const McpAction action = dispatcher.handle(kRequestId, rpc("initialize", 4));

    EXPECT_EQ(resultOf(action).value("protocolVersion").toString(), "2025-11-25");
}

TEST(McpDispatcherTest, AcknowledgesEveryNotificationWithAnEmptyResultAndNoId)
{
    const McpDispatcher dispatcher(ToolsMap {});

    for (const QString& method : { "notifications/initialized", "notifications/cancelled" })
    {
        const McpAction action
            = dispatcher.handle(kRequestId, rpc(method, QJsonValue(QJsonValue::Undefined)));

        EXPECT_EQ(action.kind, McpAction::Kind::Reply) << method.toStdString();
        EXPECT_EQ(action.reply, (QJsonObject { { "jsonrpc", "2.0" }, { "result", QJsonObject() } }))
            << method.toStdString();
    }
}

TEST(McpDispatcherTest, ListsTheToolsOfTheMapWithNameDescriptionAndInputSchema)
{
    const McpDispatcher dispatcher(toolsMapOf({ "courses_list", "facts_add" }));

    const McpAction action = dispatcher.handle(kRequestId, rpc("tools/list", 1));

    EXPECT_EQ(action.kind, McpAction::Kind::Reply);
    EXPECT_EQ(action.reply.value("id").toInt(-1), 1);
    const QJsonArray tools = resultOf(action).value("tools").toArray();
    ASSERT_EQ(tools.size(), 2);
    const QJsonObject schema { { "type", "object" },
                               { "properties", QJsonObject() },
                               { "additionalProperties", false } };
    EXPECT_EQ(tools[0].toObject(),
              (QJsonObject { { "name", "courses_list" },
                             { "description", "Does courses_list." },
                             { "inputSchema", schema } }));
    EXPECT_EQ(tools[1].toObject().value("name").toString(), "facts_add");
}

TEST(McpDispatcherTest, ListsNoToolsForAnEmptyMap)
{
    const McpDispatcher dispatcher(ToolsMap {});

    const McpAction action = dispatcher.handle(kRequestId, rpc("tools/list", 1));

    EXPECT_EQ(resultOf(action), (QJsonObject { { "tools", QJsonArray() } }));
}

TEST(McpDispatcherTest, OmitsTheDescriptionOfAToolThatHasNone)
{
    ToolsMap toolsMap;
    toolsMap.insert(
        "bare",
        ToolData { .tool = nullptr,
                   .json = QJsonObject { { "function", QJsonObject { { "name", "bare" } } } } });
    const McpDispatcher dispatcher(toolsMap);

    const McpAction action = dispatcher.handle(kRequestId, rpc("tools/list", 1));

    const QJsonObject tool = resultOf(action).value("tools").toArray().at(0).toObject();
    EXPECT_EQ(tool.value("name").toString(), "bare");
    EXPECT_FALSE(tool.contains("description"));
}

TEST(McpDispatcherTest, TurnsAToolCallIntoAToolCallOfTheAgent)
{
    const McpDispatcher dispatcher(toolsMapOf({ "facts_add" }));

    const McpAction action = dispatcher.handle(
        "req-77",
        rpc("tools/call", 2,
            callParams("facts_add", { { "title", "Kernel" }, { "count", 3 }, { "deep", true } })));

    EXPECT_EQ(action.kind, McpAction::Kind::CallTool);
    EXPECT_EQ(action.toolCall.id, "req-77");
    EXPECT_EQ(action.toolCall.name, "facts_add");
    EXPECT_EQ(action.toolCall.type, "function");
    EXPECT_EQ(action.toolCall.arguments.value("title").toString(), "Kernel");
    EXPECT_EQ(action.toolCall.arguments.value("count").toInt(), 3);
    EXPECT_TRUE(action.toolCall.arguments.value("deep").toBool());
    EXPECT_EQ(action.rpcId.toInt(-1), 2);
}

TEST(McpDispatcherTest, KeepsAStringRpcIdOfAToolCall)
{
    const McpDispatcher dispatcher(toolsMapOf({ "facts_add" }));

    const McpAction action
        = dispatcher.handle(kRequestId, rpc("tools/call", "abc", callParams("facts_add")));

    EXPECT_EQ(action.kind, McpAction::Kind::CallTool);
    EXPECT_EQ(action.rpcId.toString(), "abc");
}

TEST(McpDispatcherTest, CallsAToolWithoutArgumentsWithAnEmptyMap)
{
    const McpDispatcher dispatcher(toolsMapOf({ "courses_list" }));

    const McpAction action
        = dispatcher.handle(kRequestId, rpc("tools/call", 2, callParams("courses_list")));

    EXPECT_EQ(action.kind, McpAction::Kind::CallTool);
    EXPECT_TRUE(action.toolCall.arguments.isEmpty());
}

TEST(McpDispatcherTest, AnswersAToolCallOfAnUnknownToolWithAnErrorResultLocally)
{
    const McpDispatcher dispatcher(toolsMapOf({ "courses_list" }));

    const McpAction action
        = dispatcher.handle(kRequestId, rpc("tools/call", 5, callParams("nope")));

    EXPECT_EQ(action.kind, McpAction::Kind::Reply);
    EXPECT_EQ(action.reply.value("id").toInt(-1), 5);
    EXPECT_TRUE(resultOf(action).value("isError").toBool());
    EXPECT_EQ(resultOf(action).value("content").toArray().at(0).toObject().value("text").toString(),
              "Unknown tool: nope");
}

TEST(McpDispatcherTest, AnswersAToolCallWithoutANameAsAnUnknownTool)
{
    const McpDispatcher dispatcher(toolsMapOf({ "courses_list" }));

    const McpAction action = dispatcher.handle(kRequestId, rpc("tools/call", 5));

    EXPECT_EQ(action.kind, McpAction::Kind::Reply);
    EXPECT_TRUE(resultOf(action).value("isError").toBool());
}

TEST(McpDispatcherTest, AnswersAnUnknownMethodWithMethodNotFound)
{
    const McpDispatcher dispatcher(ToolsMap {});

    const McpAction action = dispatcher.handle(kRequestId, rpc("resources/list", 9));

    EXPECT_EQ(action.kind, McpAction::Kind::Reply);
    EXPECT_EQ(action.reply.value("id").toInt(-1), 9);
    EXPECT_EQ(errorOf(action).value("code").toInt(), -32601);
    EXPECT_EQ(errorOf(action).value("message").toString(), "Method not found");
    EXPECT_FALSE(action.reply.contains("result"));
}

TEST(McpDispatcherTest, AnswersAnUnknownMethodWithoutAnIdWithANullId)
{
    const McpDispatcher dispatcher(ToolsMap {});

    const McpAction action
        = dispatcher.handle(kRequestId, rpc("resources/list", QJsonValue(QJsonValue::Undefined)));

    EXPECT_TRUE(action.reply.contains("id"));
    EXPECT_TRUE(action.reply.value("id").isNull());
    EXPECT_EQ(errorOf(action).value("code").toInt(), -32601);
}

TEST(McpDispatcherTest, AnswersAMessageWithoutAMethodWithMethodNotFound)
{
    const McpDispatcher dispatcher(ToolsMap {});

    const McpAction action = dispatcher.handle(kRequestId, QJsonObject { { "id", 3 } });

    EXPECT_EQ(errorOf(action).value("code").toInt(), -32601);
}

TEST(McpDispatcherTest, AnswersAMessageThatIsNotAnObjectWithInvalidRequest)
{
    const McpDispatcher dispatcher(ToolsMap {});

    for (const QJsonValue& message :
         { QJsonValue(QJsonArray { 1, 2 }), QJsonValue("text"), QJsonValue(QJsonValue::Null),
           QJsonValue(QJsonValue::Undefined) })
    {
        const McpAction action = dispatcher.handle(kRequestId, message);

        EXPECT_EQ(action.kind, McpAction::Kind::Reply);
        EXPECT_TRUE(action.reply.value("id").isNull());
        EXPECT_EQ(errorOf(action).value("code").toInt(), -32600);
        EXPECT_EQ(errorOf(action).value("message").toString(), "Invalid Request");
    }
}

TEST(McpDispatcherTest, WrapsTheTextOfAToolResultInAContentBlock)
{
    const QString text = QString::fromUtf8("Zażółć\ngęślą jaźń");

    const QJsonObject reply = McpDispatcher::toolResponse(2, text);

    EXPECT_EQ(
        reply,
        (QJsonObject {
            { "jsonrpc", "2.0" },
            { "id", 2 },
            { "result",
              QJsonObject { { "content",
                              QJsonArray { QJsonObject { { "type", "text" }, { "text", text } } } },
                            { "isError", false } } } }));
}

TEST(McpDispatcherTest, AddsTheImagesOfAToolResultAfterItsText)
{
    const QByteArray bytes("\x89PNG\r\n\x1a\n-pixels-");
    const ToolImage image { QStringLiteral("caption"), QStringLiteral("image/png"), bytes };

    const QJsonObject reply
        = McpDispatcher::toolResponse(2, QStringLiteral("shown"), false, { image });

    const QJsonArray content = reply.value("result").toObject().value("content").toArray();
    ASSERT_EQ(content.size(), 2);
    EXPECT_EQ(content[0].toObject(), (QJsonObject { { "type", "text" }, { "text", "shown" } }));
    EXPECT_EQ(content[1].toObject(),
              (QJsonObject { { "type", "image" },
                             { "data", QString::fromLatin1(bytes.toBase64()) },
                             { "mimeType", "image/png" } }));
}

TEST(McpDispatcherTest, FlagsAToolResultAsAnError)
{
    const QJsonObject reply = McpDispatcher::toolResponse("abc", "boom", true);

    EXPECT_EQ(reply.value("id").toString(), "abc");
    EXPECT_TRUE(reply.value("result").toObject().value("isError").toBool());
}

TEST(McpDispatcherTest, RepliesLikeTheHostOfARecordedRun)
{
    const QList<QJsonObject> requests = recordedRequests("stdio-tool-run.jsonl");
    const QList<QJsonObject> replies = recordedReplies("stdio-tool-run.jsonl");
    ASSERT_EQ(requests.size(), 4);
    ASSERT_EQ(replies.size(), 4);
    ToolsMap toolsMap;
    toolsMap.insert("courses_list",
                    toolData("courses_list", "Lists the titles of the user's courses."));
    const McpDispatcher dispatcher(toolsMap);

    for (qsizetype at = 0; at < 3; ++at)
    {
        const McpAction action
            = dispatcher.handle(requests[at].value("request_id").toString(),
                                requests[at].value("request").toObject().value("message"));

        EXPECT_EQ(action.kind, McpAction::Kind::Reply) << "request " << at;
        EXPECT_EQ(action.reply, replies[at]) << "request " << at;
    }

    const McpAction call
        = dispatcher.handle(requests[3].value("request_id").toString(),
                            requests[3].value("request").toObject().value("message"));
    ASSERT_EQ(call.kind, McpAction::Kind::CallTool);
    EXPECT_EQ(call.toolCall.name, "courses_list");
    EXPECT_EQ(McpDispatcher::toolResponse(call.rpcId, "Linux Kernel Basics\nC++ Memory Model"),
              replies[3]);
}
