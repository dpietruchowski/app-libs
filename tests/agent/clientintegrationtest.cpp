#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include "agent/agent.h"
#include "agent/client.h"
#include "loopbackservertest.h"

class ClientIntegrationTest : public LoopbackServerTest
{
protected:
    void SetUp() override
    {
        LoopbackServerTest::SetUp();
        responseBody
            = R"({"id":"cmpl-x","object":"chat.completion","created":1,"model":"mock-model",)"
              R"("usage":{"prompt_tokens":1,"completion_tokens":1,"total_tokens":2},)"
              R"("choices":[{"index":0,"finish_reason":"stop",)"
              R"("message":{"role":"assistant","content":"mock reply"}}]})";
    }

    QString url() const { return LoopbackServerTest::url("/v1/chat/completions"); }
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

TEST_F(ClientIntegrationTest, CreateCompletion_SendsReasoningEffortInLowerCase)
{
    Client client(url(), "test-key");

    client.createCompletion(ModelConfig { .model = "gpt-test", .reasoningEffort = "Minimal" },
                            Messages {}, ToolsMap {});

    const QJsonObject body = requestJson();
    EXPECT_EQ(body.value("reasoning_effort").toString(), "minimal");
    EXPECT_FALSE(body.contains("reasoning"));
}

TEST_F(ClientIntegrationTest, CreateCompletion_SendsNestedReasoningEffortInLowerCase)
{
    Client client(url(), "test-key");
    client.setReasoningNested(true);

    client.createCompletion(ModelConfig { .model = "gpt-test", .reasoningEffort = "High" },
                            Messages {}, ToolsMap {});

    const QJsonObject body = requestJson();
    EXPECT_EQ(body.value("reasoning").toObject().value("effort").toString(), "high");
    EXPECT_FALSE(body.contains("reasoning_effort"));
}

TEST_F(ClientIntegrationTest, CreateCompletion_OmitsReasoningEffortWhenNoneIsSet)
{
    Client client(url(), "test-key");

    client.createCompletion(ModelConfig { .model = "gpt-test" }, Messages {}, ToolsMap {});

    const QJsonObject body = requestJson();
    EXPECT_FALSE(body.contains("reasoning_effort"));
    EXPECT_FALSE(body.contains("reasoning"));
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
