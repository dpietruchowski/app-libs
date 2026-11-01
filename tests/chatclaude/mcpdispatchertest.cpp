#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include "chatclaude/httprequestparser.h"
#include "chatclaude/mcpdispatcher.h"
#include "recordedrequests.h"
#include "testcatalog.h"

using namespace chat::claude;
using chat::test::recordedRequests;
using chat::test::wireFormat;
using chat::test::withPath;

namespace
{

HttpResponse await(Task<HttpResponse> task)
{
    const Result<HttpResponse> result = task.future().result();
    EXPECT_TRUE(result.isSuccess());
    return result.isSuccess() ? result.value() : HttpResponse {};
}

QJsonObject bodyOf(const HttpResponse& response)
{
    return QJsonDocument::fromJson(response.body).object();
}

HttpRequest recorded(const QJsonObject& entry, const QString& agentKey)
{
    HttpRequestParser parser;
    parser.feed(wireFormat(withPath(entry, "/mcp/" + agentKey)));
    return parser.takeRequest().value_or(HttpRequest {});
}

HttpRequest recordedCliRequest(int index, const QString& agentKey = "courses")
{
    return recorded(recordedRequests("sync-mcp-requests.log").at(index), agentKey);
}

HttpRequest postTo(const QByteArray& path, const QByteArray& body)
{
    return HttpRequest { .method = "POST",
                         .path = path,
                         .headers = { { "Authorization", "Bearer test-token" } },
                         .body = body };
}

QJsonObject message(const QString& method, const QJsonObject& params = {}, const QJsonValue& id = 1)
{
    return { { "jsonrpc", "2.0" }, { "id", id }, { "method", method }, { "params", params } };
}

HttpRequest rpc(const QJsonObject& message, const QByteArray& path = "/mcp/courses")
{
    return postTo(path, QJsonDocument(message).toJson(QJsonDocument::Compact));
}

QJsonObject toolCall(const QString& name, const QJsonObject& arguments = {})
{
    return message("tools/call", { { "name", name }, { "arguments", arguments } });
}

QJsonValue textOf(const HttpResponse& response)
{
    const QJsonArray content
        = bodyOf(response).value("result").toObject().value("content").toArray();
    EXPECT_EQ(content.size(), 1);
    EXPECT_EQ(content.at(0).toObject().value("type").toString(), "text");
    return content.at(0).toObject().value("text");
}

bool isToolError(const HttpResponse& response)
{
    return bodyOf(response).value("result").toObject().value("isError").toBool();
}

QStringList toolNames(const HttpResponse& response)
{
    QStringList names;
    for (const QJsonValue& tool :
         bodyOf(response).value("result").toObject().value("tools").toArray())
        names.append(tool.toObject().value("name").toString());
    return names;
}

}

class McpDispatcherTest : public ::testing::Test
{
protected:
    HttpResponse send(const HttpRequest& request) { return await(dispatcher.dispatch(request)); }

    chat::test::DeferredCalls deferred;
    chat::AgentCatalog catalog = chat::test::makeCatalog(deferred);
    McpDispatcher dispatcher { catalog, "test-token" };
};

TEST_F(McpDispatcherTest, ReplaysTheRecordedCliSequenceWithTheStatusesTheCliGot)
{
    const QList<QJsonObject> entries = recordedRequests("sync-mcp-requests.log");
    ASSERT_EQ(entries.size(), 6);

    for (const QJsonObject& entry : entries)
    {
        EXPECT_EQ(send(recorded(entry, "courses")).status, entry.value("status").toInt())
            << entry.value("body").toString().toStdString();
    }
}

TEST_F(McpDispatcherTest, RejectsTheRecordedWrongTokenSequenceWith403)
{
    const QList<QJsonObject> entries = recordedRequests("mcp-403-requests.log");
    ASSERT_EQ(entries.size(), 2);

    for (const QJsonObject& entry : entries)
    {
        const HttpResponse response = send(recorded(entry, "courses"));

        EXPECT_EQ(response.status, entry.value("status").toInt());
        EXPECT_TRUE(response.body.isEmpty());
    }
}

TEST_F(McpDispatcherTest, RejectsARequestWithoutAuthorization)
{
    HttpRequest request = rpc(message("tools/list"));
    request.headers.clear();

    EXPECT_EQ(send(request).status, 403);
}

TEST_F(McpDispatcherTest, ChecksTheTokenBeforeTheAgent)
{
    HttpRequest request = rpc(message("tools/list"), "/mcp/ghost");
    request.headers = { { "Authorization", "Bearer wrong-token" } };

    EXPECT_EQ(send(request).status, 403);
}

TEST_F(McpDispatcherTest, DoesNotCallAnyToolForAWrongToken)
{
    HttpRequest request = rpc(toolCall("courses_slow"));
    request.headers = { { "Authorization", "Bearer wrong-token" } };

    EXPECT_EQ(send(request).status, 403);
    EXPECT_EQ(deferred.pending(), 0u);
}

TEST_F(McpDispatcherTest, AnswersServerDiscoverWithMethodNotFound)
{
    const HttpResponse response = send(recordedCliRequest(0));

    EXPECT_EQ(response.status, 200);
    EXPECT_EQ(response.headers.at(0).value, QByteArray("application/json"));
    const QJsonObject body = bodyOf(response);
    EXPECT_EQ(body.value("jsonrpc").toString(), "2.0");
    EXPECT_EQ(body.value("id").toString(), "server-discover-probe-1");
    EXPECT_EQ(body.value("error").toObject().value("code").toInt(), -32601);
    EXPECT_FALSE(body.contains("result"));
}

TEST_F(McpDispatcherTest, EchoesTheRequestedProtocolVersionOnInitialize)
{
    const QJsonObject body = bodyOf(send(recordedCliRequest(1)));

    EXPECT_EQ(body.value("id").toInt(), 0);
    const QJsonObject result = body.value("result").toObject();
    EXPECT_EQ(result.value("protocolVersion").toString(), "2025-11-25");
    EXPECT_TRUE(result.value("capabilities").toObject().value("tools").isObject());
    EXPECT_FALSE(result.value("serverInfo").toObject().value("name").toString().isEmpty());
}

TEST_F(McpDispatcherTest, EchoesWhateverProtocolVersionTheClientAsksFor)
{
    const QJsonObject body
        = bodyOf(send(rpc(message("initialize", { { "protocolVersion", "2099-01-01" } }))));

    EXPECT_EQ(body.value("result").toObject().value("protocolVersion").toString(), "2099-01-01");
}

TEST_F(McpDispatcherTest, OffersAProtocolVersionWhenTheClientNamesNone)
{
    const QJsonObject body = bodyOf(send(rpc(message("initialize"))));

    EXPECT_FALSE(body.value("result").toObject().value("protocolVersion").toString().isEmpty());
}

TEST_F(McpDispatcherTest, AcceptsNotificationsWithoutABody)
{
    const HttpResponse response = send(recordedCliRequest(2));

    EXPECT_EQ(response.status, 202);
    EXPECT_TRUE(response.body.isEmpty());
}

TEST_F(McpDispatcherTest, RefusesTheEventStreamWith405)
{
    const HttpResponse response = send(recordedCliRequest(3));

    EXPECT_EQ(response.status, 405);
    EXPECT_TRUE(response.body.isEmpty());
    EXPECT_EQ(response.headers.at(0).name, QByteArray("Allow"));
    EXPECT_EQ(response.headers.at(0).value, QByteArray("POST"));
}

TEST_F(McpDispatcherTest, AnswersPingWithAnEmptyResult)
{
    const QJsonObject body = bodyOf(send(rpc(message("ping", {}, 7))));

    EXPECT_EQ(body.value("id").toInt(), 7);
    EXPECT_TRUE(body.value("result").toObject().isEmpty());
}

TEST_F(McpDispatcherTest, ListsOnlyTheToolsOfTheAddressedAgent)
{
    EXPECT_EQ(toolNames(send(recordedCliRequest(4, "courses"))),
              (QStringList { "courses_list", "courses_echo", "courses_fail", "courses_slow" }));
    EXPECT_EQ(toolNames(send(recordedCliRequest(4, "lessons"))), QStringList { "lessons_read" });
}

TEST_F(McpDispatcherTest, DescribesEachToolWithItsInputSchema)
{
    const QJsonObject body = bodyOf(send(recordedCliRequest(4, "lessons")));

    const QJsonObject tool
        = body.value("result").toObject().value("tools").toArray().at(0).toObject();
    EXPECT_EQ(tool.value("description").toString(), "Reads a lesson");
    const QJsonObject schema = tool.value("inputSchema").toObject();
    EXPECT_EQ(schema.value("type").toString(), "object");
    EXPECT_EQ(
        schema.value("properties").toObject().value("lessonId").toObject().value("type").toString(),
        "integer");
}

TEST_F(McpDispatcherTest, CallsTheToolAndReturnsItsValueAsJsonText)
{
    const HttpResponse response = send(recordedCliRequest(5));

    EXPECT_EQ(response.status, 200);
    EXPECT_EQ(bodyOf(response).value("id").toInt(), 2);
    EXPECT_FALSE(isToolError(response));
    EXPECT_EQ(QJsonDocument::fromJson(textOf(response).toString().toUtf8()).array(),
              chat::test::coursesPayload());
}

TEST_F(McpDispatcherTest, ReturnsAStringResultAsPlainText)
{
    const HttpResponse response
        = send(rpc(toolCall("lessons_read", { { "lessonId", 3 } }), "/mcp/lessons"));

    EXPECT_FALSE(isToolError(response));
    EXPECT_EQ(textOf(response).toString(), "Lesson text");
}

TEST_F(McpDispatcherTest, PassesTheArgumentsToTheTool)
{
    const HttpResponse response = send(rpc(toolCall("courses_echo", { { "text", "hello" } })));

    EXPECT_EQ(QJsonDocument::fromJson(textOf(response).toString().toUtf8()).object(),
              (QJsonObject { { "text", "hello" } }));
}

TEST_F(McpDispatcherTest, GivesAToolCalledWithoutArgumentsAnEmptyObject)
{
    const HttpResponse response = send(rpc(message("tools/call", { { "name", "courses_echo" } })));

    EXPECT_FALSE(isToolError(response));
    EXPECT_EQ(textOf(response).toString(), "{}");
}

TEST_F(McpDispatcherTest, ReportsAFailingToolAsAToolErrorNotAProtocolError)
{
    const HttpResponse response = send(rpc(toolCall("courses_fail")));

    EXPECT_EQ(response.status, 200);
    EXPECT_TRUE(isToolError(response));
    EXPECT_EQ(textOf(response).toString(), "Course store is offline");
    EXPECT_FALSE(bodyOf(response).contains("error"));
}

TEST_F(McpDispatcherTest, ReportsAnUnknownToolAsAToolError)
{
    const HttpResponse response = send(rpc(toolCall("no_such_tool")));

    EXPECT_EQ(response.status, 200);
    EXPECT_TRUE(isToolError(response));
    EXPECT_TRUE(textOf(response).toString().contains("no_such_tool"));
}

TEST_F(McpDispatcherTest, DoesNotCallAToolOfAnotherAgent)
{
    const HttpResponse response = send(rpc(toolCall("lessons_read"), "/mcp/courses"));

    EXPECT_TRUE(isToolError(response));
    EXPECT_TRUE(textOf(response).toString().contains("lessons_read"));
}

TEST_F(McpDispatcherTest, AnswersASlowToolOnlyWhenItFinishes)
{
    Task<HttpResponse> task = dispatcher.dispatch(rpc(toolCall("courses_slow"), "/mcp/courses"));

    EXPECT_EQ(deferred.pending(), 1u);
    EXPECT_FALSE(task.future().isFinished());

    deferred.complete(0, "done");
    const HttpResponse response = await(task);

    EXPECT_EQ(response.status, 200);
    EXPECT_EQ(textOf(response).toString(), "done");
}

TEST_F(McpDispatcherTest, ReturnsANonStringResultAsCompactJson)
{
    Task<HttpResponse> numberTask = dispatcher.dispatch(rpc(toolCall("courses_slow")));
    Task<HttpResponse> objectTask = dispatcher.dispatch(rpc(toolCall("courses_slow")));

    deferred.complete(0, 42);
    deferred.complete(1, QJsonObject { { "id", 7 }, { "title", "Networking" } });

    EXPECT_EQ(textOf(await(numberTask)).toString(), "42");
    EXPECT_EQ(textOf(await(objectTask)).toString(), "{\"id\":7,\"title\":\"Networking\"}");
}

TEST_F(McpDispatcherTest, ReportsASlowToolThatFailsLateAsAToolError)
{
    Task<HttpResponse> task = dispatcher.dispatch(rpc(toolCall("courses_slow")));

    deferred.fail(0, "Timed out");
    const HttpResponse response = await(task);

    EXPECT_TRUE(isToolError(response));
    EXPECT_EQ(textOf(response).toString(), "Timed out");
}

TEST_F(McpDispatcherTest, KeepsParallelCallsApart)
{
    Task<HttpResponse> first
        = dispatcher.dispatch(rpc(message("tools/call", { { "name", "courses_slow" } }, 11)));
    Task<HttpResponse> second
        = dispatcher.dispatch(rpc(message("tools/call", { { "name", "courses_slow" } }, 12)));

    deferred.complete(1, "second");
    deferred.complete(0, "first");

    const HttpResponse firstResponse = await(first);
    const HttpResponse secondResponse = await(second);
    EXPECT_EQ(bodyOf(firstResponse).value("id").toInt(), 11);
    EXPECT_EQ(textOf(firstResponse).toString(), "first");
    EXPECT_EQ(bodyOf(secondResponse).value("id").toInt(), 12);
    EXPECT_EQ(textOf(secondResponse).toString(), "second");
}

TEST_F(McpDispatcherTest, RejectsAToolCallWithoutAToolName)
{
    const HttpResponse response
        = send(rpc(message("tools/call", { { "arguments", QJsonObject() } })));

    EXPECT_EQ(response.status, 200);
    EXPECT_EQ(bodyOf(response).value("error").toObject().value("code").toInt(), -32602);
}

TEST_F(McpDispatcherTest, RejectsToolArgumentsThatAreNotAnObject)
{
    const HttpResponse response
        = send(rpc(message("tools/call", { { "name", "courses_echo" }, { "arguments", "text" } })));

    EXPECT_EQ(bodyOf(response).value("error").toObject().value("code").toInt(), -32602);
    EXPECT_EQ(deferred.pending(), 0u);
}

TEST_F(McpDispatcherTest, AnswersAnUnknownMethodWithMethodNotFoundAndTheRequestId)
{
    const QJsonObject body = bodyOf(send(rpc(message("resources/list", {}, 9))));

    EXPECT_EQ(body.value("id").toInt(), 9);
    EXPECT_EQ(body.value("error").toObject().value("code").toInt(), -32601);
}

TEST_F(McpDispatcherTest, AnswersMalformedJsonWithAParseError)
{
    const HttpResponse response = send(postTo("/mcp/courses", "{\"jsonrpc\":"));

    EXPECT_EQ(response.status, 400);
    const QJsonObject body = bodyOf(response);
    EXPECT_EQ(body.value("error").toObject().value("code").toInt(), -32700);
    EXPECT_TRUE(body.value("id").isNull());
}

TEST_F(McpDispatcherTest, AnswersBodiesThatAreNotASingleJsonValueWithAParseError)
{
    for (const QByteArray body :
         { QByteArray(), QByteArray("   "), QByteArray("not json"), QByteArray("[1"),
           QByteArray("1,2"), QByteArray("42 43"), QByteArray("{} {}"), QByteArray("1],[2") })
    {
        const HttpResponse response = send(postTo("/mcp/courses", body));

        EXPECT_EQ(response.status, 400) << body.toStdString();
        EXPECT_EQ(bodyOf(response).value("error").toObject().value("code").toInt(), -32700)
            << body.toStdString();
    }
}

TEST_F(McpDispatcherTest, RefusesBatchesAndOtherNonObjectBodies)
{
    for (const QByteArray body :
         { QByteArray("[]"), QByteArray("[{}]"), QByteArray("42"), QByteArray("-1.5e3"),
           QByteArray("\"text\""), QByteArray("true"), QByteArray("null"), QByteArray(" 42 ") })
    {
        const HttpResponse response = send(postTo("/mcp/courses", body));

        EXPECT_EQ(response.status, 400) << body.toStdString();
        EXPECT_EQ(bodyOf(response).value("error").toObject().value("code").toInt(), -32600)
            << body.toStdString();
    }
}

TEST_F(McpDispatcherTest, AnswersAnUnknownAgentWith404)
{
    for (const QByteArray path : { QByteArray("/mcp/ghost"), QByteArray("/mcp/"),
                                   QByteArray("/mcp"), QByteArray("/elsewhere/courses") })
    {
        EXPECT_EQ(send(rpc(message("tools/list"), path)).status, 404) << path.toStdString();
    }
}

TEST_F(McpDispatcherTest, ChecksTheAgentBeforeTheMethod)
{
    HttpRequest request = rpc(message("tools/list"), "/mcp/ghost");
    request.method = "GET";

    EXPECT_EQ(send(request).status, 404);
}
