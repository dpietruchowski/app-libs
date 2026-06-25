#include <gtest/gtest.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include "agent/agent.h"
#include "agent/client.h"

namespace
{
int contentLength(const QByteArray& headers)
{
    for (const QByteArray& line : headers.split('\n'))
    {
        QByteArray trimmed = line.trimmed();
        if (trimmed.toLower().startsWith("content-length:"))
        {
            return trimmed.mid(trimmed.indexOf(':') + 1).trimmed().toInt();
        }
    }
    return 0;
}
}

// Spins up a loopback HTTP server that replies with a canned completion, so the
// real Client (network request building + reply parsing) can be exercised
// without touching OpenAI.
class ClientIntegrationTest : public ::testing::Test
{
protected:
    QTcpServer server;
    QByteArray responseBody;
    QByteArray lastRequest;

    void SetUp() override
    {
        responseBody = R"({"id":"cmpl-x","object":"chat.completion","created":1,"model":"mock-model",)"
                       R"("usage":{"prompt_tokens":1,"completion_tokens":1,"total_tokens":2},)"
                       R"("choices":[{"index":0,"finish_reason":"stop",)"
                       R"("message":{"role":"assistant","content":"mock reply"}}]})";

        ASSERT_TRUE(server.listen(QHostAddress::LocalHost, 0));

        QObject::connect(&server, &QTcpServer::newConnection, &server,
                         [this]
                         {
                             QTcpSocket* socket = server.nextPendingConnection();
                             QObject::connect(socket, &QTcpSocket::readyRead, socket,
                                              [this, socket] { onReadyRead(socket); });
                         });
    }

    void onReadyRead(QTcpSocket* socket)
    {
        lastRequest += socket->readAll();

        int headerEnd = lastRequest.indexOf("\r\n\r\n");
        if (headerEnd < 0)
        {
            return;
        }
        QByteArray headers = lastRequest.left(headerEnd);
        QByteArray body = lastRequest.mid(headerEnd + 4);
        if (body.size() < contentLength(headers))
        {
            return; // wait for the rest of the body
        }

        QByteArray response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: "
                              + QByteArray::number(responseBody.size())
                              + "\r\n"
                                "Connection: close\r\n\r\n"
                              + responseBody;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }

    QString url() const
    {
        return QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort());
    }
};

TEST_F(ClientIntegrationTest, CreateCompletion_ParsesServerReply)
{
    Client client(url(), "test-key");

    Messages messages;
    messages.append(Message { .role = "user", .content = "hi" });

    Completion completion =
        client.createCompletion(ModelConfig { .model = "gpt-test" }, messages, ToolsMap {});

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices[0].message.content, "mock reply");
    EXPECT_EQ(completion.model, "mock-model");
}

TEST_F(ClientIntegrationTest, CreateCompletion_SendsModelAndMessagesInRequestBody)
{
    Client client(url(), "test-key");

    Messages messages;
    messages.append(Message { .role = "user", .content = "ping" });

    client.createCompletion(ModelConfig { .model = "gpt-test" }, messages, ToolsMap {});

    EXPECT_TRUE(lastRequest.contains("gpt-test"));
    EXPECT_TRUE(lastRequest.contains("ping"));
    EXPECT_TRUE(lastRequest.contains("Bearer test-key"));
}

TEST_F(ClientIntegrationTest, Agent_RequestThroughRealClient_ReturnsContent)
{
    Client client(url(), "test-key");

    Agent agent("gpt-test", "system prompt");

    QString response;
    bool finished = false;
    agent.requestAsync(client, "hello",
                       [&](const QString& r)
                       {
                           response = r;
                           finished = true;
                       });

    QElapsedTimer timer;
    timer.start();
    while (!finished && timer.elapsed() < 2000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    EXPECT_EQ(response, "mock reply");
}
