#include <gtest/gtest.h>

#include <QAbstractSocket>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTest>
#include <optional>

#include "chatclaude/toolbridge.h"
#include "recordedrequests.h"
#include "testcatalog.h"

using namespace chat::claude;
using chat::test::recordedRequests;
using chat::test::wireFormat;
using chat::test::wireFormatOf;
using chat::test::withHeader;
using chat::test::withPath;

namespace
{

struct Reply
{
    int status { 0 };
    QByteArray head;
    QByteArray body;

    QJsonObject json() const { return QJsonDocument::fromJson(body).object(); }

    QString text() const
    {
        return json()
            .value("result")
            .toObject()
            .value("content")
            .toArray()
            .at(0)
            .toObject()
            .value("text")
            .toString();
    }
};

class Client
{
public:
    explicit Client(quint16 port)
    {
        QObject::connect(&m_socket, &QTcpSocket::readyRead, &m_socket,
                         [this] { m_buffer += m_socket.readAll(); });
        m_socket.connectToHost(QHostAddress::LocalHost, port);
        static_cast<void>(QTest::qWaitFor(
            [this]
            {
                return m_socket.state() == QAbstractSocket::ConnectedState
                    || m_socket.state() == QAbstractSocket::UnconnectedState;
            }));
    }

    bool isConnected() const { return m_socket.state() == QAbstractSocket::ConnectedState; }

    bool waitForClose()
    {
        return QTest::qWaitFor([this]
                               { return m_socket.state() == QAbstractSocket::UnconnectedState; });
    }

    void close() { m_socket.abort(); }

    void send(const QByteArray& bytes)
    {
        m_socket.write(bytes);
        m_socket.flush();
    }

    std::optional<Reply> takeReply()
    {
        const qsizetype headEnd = m_buffer.indexOf("\r\n\r\n");
        if (headEnd < 0)
            return std::nullopt;

        const QByteArray head = m_buffer.left(headEnd);
        qsizetype length = 0;
        for (const QByteArray& line : head.split('\n'))
        {
            if (line.trimmed().toLower().startsWith("content-length:"))
                length = line.trimmed().mid(15).trimmed().toLongLong();
        }

        const qsizetype total = headEnd + 4 + length;
        if (m_buffer.size() < total)
            return std::nullopt;

        Reply reply;
        reply.status = head.split(' ').value(1).toInt();
        reply.head = head;
        reply.body = m_buffer.mid(headEnd + 4, length);
        m_buffer.remove(0, total);
        return reply;
    }

    std::optional<Reply> receive()
    {
        std::optional<Reply> reply;
        static_cast<void>(QTest::qWaitFor(
            [&]
            {
                if (!reply)
                    reply = takeReply();
                return reply.has_value();
            }));
        return reply;
    }

private:
    QTcpSocket m_socket;
    QByteArray m_buffer;
};

}

class ToolBridgeTest : public ::testing::Test
{
protected:
    void SetUp() override { ASSERT_TRUE(bridge.start()); }

    QByteArray wire(const QJsonObject& entry, const QString& agentKey = "courses") const
    {
        return wireFormat(withHeader(withPath(entry, "/mcp/" + agentKey), "Authorization",
                                     "Bearer " + QString::fromLatin1(bridge.token())));
    }

    QByteArray toolCallWire(const QString& tool, int id) const
    {
        const QJsonObject message { { "jsonrpc", "2.0" },
                                    { "id", id },
                                    { "method", "tools/call" },
                                    { "params", QJsonObject { { "name", tool } } } };
        const QByteArray body = QJsonDocument(message).toJson(QJsonDocument::Compact);
        return "POST /mcp/courses HTTP/1.1\r\nAuthorization: Bearer " + bridge.token()
            + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    }

    QByteArray toolListWire() const
    {
        return wire(recordedRequests("sync-mcp-requests.log").at(4));
    }

    chat::test::DeferredCalls deferred;
    chat::AgentCatalog catalog = chat::test::makeCatalog(deferred);
    ToolBridge bridge { catalog };
};

TEST_F(ToolBridgeTest, ListensOnLoopbackAndHandsOutAnAgentUrl)
{
    EXPECT_TRUE(bridge.isListening());
    EXPECT_NE(bridge.port(), 0);
    EXPECT_EQ(bridge.agentUrl("courses"),
              QStringLiteral("http://127.0.0.1:%1/mcp/courses").arg(bridge.port()));
}

TEST_F(ToolBridgeTest, DoesNotListenBeforeItIsStarted)
{
    ToolBridge idle(catalog);

    EXPECT_FALSE(idle.isListening());
}

TEST_F(ToolBridgeTest, GeneratesALongRandomTokenPerBridge)
{
    ToolBridge other(catalog);

    EXPECT_EQ(bridge.token().size(), 64);
    EXPECT_EQ(bridge.token(), bridge.token().toLower());
    EXPECT_NE(other.token(), bridge.token());
}

TEST_F(ToolBridgeTest, ServesTheRecordedCliSequenceOverASocket)
{
    Client client(bridge.port());
    ASSERT_TRUE(client.isConnected());

    for (const QJsonObject& entry : recordedRequests("sync-mcp-requests.log"))
    {
        client.send(wire(entry));
        const std::optional<Reply> reply = client.receive();

        ASSERT_TRUE(reply.has_value()) << entry.value("body").toString().toStdString();
        EXPECT_EQ(reply->status, entry.value("status").toInt())
            << entry.value("body").toString().toStdString();
    }
}

TEST_F(ToolBridgeTest, AnswersAToolCallWithTheToolResult)
{
    Client client(bridge.port());
    client.send(wire(recordedRequests("sync-mcp-requests.log").at(5)));

    const std::optional<Reply> reply = client.receive();

    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->json().value("id").toInt(), 2);
    EXPECT_EQ(QJsonDocument::fromJson(reply->text().toUtf8()).array(),
              chat::test::coursesPayload());
}

TEST_F(ToolBridgeTest, AnswersWithAContentLengthAndAJsonContentType)
{
    Client client(bridge.port());
    client.send(toolListWire());

    const std::optional<Reply> reply = client.receive();

    ASSERT_TRUE(reply.has_value());
    EXPECT_TRUE(reply->head.startsWith("HTTP/1.1 200 OK\r\n"));
    EXPECT_TRUE(reply->head.contains("Content-Type: application/json\r\n"));
    EXPECT_TRUE(reply->head.contains("Content-Length: " + QByteArray::number(reply->body.size())));
}

TEST_F(ToolBridgeTest, RejectsTheRecordedWrongTokenWith403)
{
    Client client(bridge.port());
    ASSERT_TRUE(client.isConnected());

    for (const QJsonObject& entry : recordedRequests("mcp-403-requests.log"))
    {
        client.send(wireFormat(withPath(entry, "/mcp/courses")));
        const std::optional<Reply> reply = client.receive();

        ASSERT_TRUE(reply.has_value());
        EXPECT_EQ(reply->status, 403);
        EXPECT_TRUE(reply->body.isEmpty());
    }
}

TEST_F(ToolBridgeTest, AnswersARequestSentInPiecesOnceItIsComplete)
{
    Client client(bridge.port());
    const QByteArray request = toolListWire();
    const qsizetype middle = request.size() / 2;

    client.send(request.left(middle));
    QTest::qWait(30);
    EXPECT_FALSE(client.takeReply().has_value());
    client.send(request.mid(middle));

    const std::optional<Reply> reply = client.receive();
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status, 200);
}

TEST_F(ToolBridgeTest, AnswersAPipelinedBurstInOrder)
{
    const QList<QJsonObject> entries = recordedRequests("sync-mcp-requests.log");
    QByteArray burst;
    for (const QJsonObject& entry : entries)
        burst += wire(entry);
    Client client(bridge.port());

    client.send(burst);

    for (const QJsonObject& entry : entries)
    {
        const std::optional<Reply> reply = client.receive();
        ASSERT_TRUE(reply.has_value());
        EXPECT_EQ(reply->status, entry.value("status").toInt());
    }
}

TEST_F(ToolBridgeTest, AnswersASlowToolWhenItFinishesAndKeepsTheConnection)
{
    Client client(bridge.port());
    client.send(toolCallWire("courses_slow", 5));

    ASSERT_TRUE(QTest::qWaitFor([this] { return deferred.pending() == 1; }));
    EXPECT_FALSE(client.takeReply().has_value());

    deferred.complete(0, "done");
    const std::optional<Reply> slow = client.receive();
    ASSERT_TRUE(slow.has_value());
    EXPECT_EQ(slow->json().value("id").toInt(), 5);
    EXPECT_EQ(slow->text(), "done");

    client.send(toolListWire());
    const std::optional<Reply> list = client.receive();
    ASSERT_TRUE(list.has_value());
    EXPECT_EQ(list->status, 200);
}

TEST_F(ToolBridgeTest, HoldsAPipelinedRequestBehindASlowOne)
{
    Client client(bridge.port());
    client.send(toolCallWire("courses_slow", 5) + toolListWire());

    ASSERT_TRUE(QTest::qWaitFor([this] { return deferred.pending() == 1; }));
    QTest::qWait(30);
    EXPECT_FALSE(client.takeReply().has_value());

    deferred.complete(0, "done");

    const std::optional<Reply> first = client.receive();
    const std::optional<Reply> second = client.receive();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->json().value("id").toInt(), 5);
    EXPECT_TRUE(second->json().value("result").toObject().contains("tools"));
}

TEST_F(ToolBridgeTest, RunsCallsOfSeparateConnectionsInParallel)
{
    Client first(bridge.port());
    Client second(bridge.port());
    first.send(toolCallWire("courses_slow", 21));
    second.send(toolCallWire("courses_slow", 22));
    ASSERT_TRUE(QTest::qWaitFor([this] { return deferred.pending() == 2; }));

    deferred.complete(1, "second");
    const std::optional<Reply> secondReply = second.receive();
    ASSERT_TRUE(secondReply.has_value());
    EXPECT_EQ(secondReply->json().value("id").toInt(), 22);
    EXPECT_EQ(secondReply->text(), "second");
    EXPECT_FALSE(first.takeReply().has_value());

    deferred.complete(0, "first");
    const std::optional<Reply> firstReply = first.receive();
    ASSERT_TRUE(firstReply.has_value());
    EXPECT_EQ(firstReply->json().value("id").toInt(), 21);
    EXPECT_EQ(firstReply->text(), "first");
}

TEST_F(ToolBridgeTest, AnswersAnUnparsableRequestWith400AndClosesTheConnection)
{
    Client client(bridge.port());

    client.send("garbage\r\n\r\n");

    const std::optional<Reply> reply = client.receive();
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status, 400);
    EXPECT_TRUE(reply->head.contains("Connection: close\r\n"));
    EXPECT_TRUE(client.waitForClose());
}

TEST_F(ToolBridgeTest, AnswersAnOversizedBodyWith413AndClosesTheConnection)
{
    Client client(bridge.port());

    client.send("POST /mcp/courses HTTP/1.1\r\nContent-Length: 99999999\r\n\r\n");

    const std::optional<Reply> reply = client.receive();
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status, 413);
    EXPECT_TRUE(client.waitForClose());
}

TEST_F(ToolBridgeTest, AnswersOversizedHeadersWith431AndClosesTheConnection)
{
    Client client(bridge.port());

    client.send("POST /mcp/courses HTTP/1.1\r\nX-Padding: " + QByteArray(20000, 'a') + "\r\n\r\n");

    const std::optional<Reply> reply = client.receive();
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status, 431);
    EXPECT_TRUE(reply->head.contains("Connection: close\r\n"));
    EXPECT_TRUE(client.waitForClose());
}

TEST_F(ToolBridgeTest, StillAnswersOtherConnectionsAfterOneWasRejected)
{
    Client broken(bridge.port());
    broken.send("garbage\r\n\r\n");
    ASSERT_TRUE(broken.receive().has_value());
    Client healthy(bridge.port());

    healthy.send(toolListWire());

    const std::optional<Reply> reply = healthy.receive();
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status, 200);
}

TEST_F(ToolBridgeTest, SurvivesAClientThatLeavesWhileItsCallIsRunning)
{
    Client leaving(bridge.port());
    leaving.send(toolCallWire("courses_slow", 5));
    ASSERT_TRUE(QTest::qWaitFor([this] { return deferred.pending() == 1; }));

    leaving.close();
    QTest::qWait(30);
    deferred.complete(0, "too late");
    QTest::qWait(30);

    Client next(bridge.port());
    next.send(toolListWire());
    const std::optional<Reply> reply = next.receive();
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status, 200);
}

TEST_F(ToolBridgeTest, StopHangsUpOnClientsAndDropsRepliesOfCallsStillRunning)
{
    Client client(bridge.port());
    client.send(toolCallWire("courses_slow", 5));
    ASSERT_TRUE(QTest::qWaitFor([this] { return deferred.pending() == 1; }));

    bridge.stop();

    EXPECT_TRUE(client.waitForClose());
    deferred.complete(0, "too late");
    QTest::qWait(30);
    EXPECT_FALSE(client.takeReply().has_value());
}

TEST_F(ToolBridgeTest, StopRefusesNewConnections)
{
    const quint16 port = bridge.port();

    bridge.stop();

    EXPECT_FALSE(bridge.isListening());
    Client late(port);
    EXPECT_FALSE(late.isConnected());
}

TEST_F(ToolBridgeTest, ListensAgainAfterStop)
{
    bridge.stop();

    ASSERT_TRUE(bridge.start());

    Client client(bridge.port());
    client.send(toolListWire());
    const std::optional<Reply> reply = client.receive();
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->status, 200);
}
