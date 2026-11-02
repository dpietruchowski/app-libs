#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include "agent/anthropicclient.h"
#include "loopbackservertest.h"

namespace
{
const char* kMessageBody
    = R"({"id":"msg_1","type":"message","role":"assistant","model":"claude-test",)"
      R"("content":[{"type":"text","text":"mock reply"}],"stop_reason":"end_turn",)"
      R"("usage":{"input_tokens":3,"cache_creation_input_tokens":10,)"
      R"("cache_read_input_tokens":20,"output_tokens":5}})";

const char* kErrorBody = R"({"type":"error","error":{"type":"%1","message":"nope"}})";

Messages pingMessages() { return { Message { .role = "user", .content = "ping" } }; }

ModelConfig testConfig() { return ModelConfig { .model = "claude-test" }; }
}

class AnthropicTransportTest : public LoopbackServerTest
{
protected:
    void SetUp() override
    {
        LoopbackServerTest::SetUp();
        responseBody = kMessageBody;
    }

    QString url() const { return LoopbackServerTest::url("/v1/messages"); }

    Completion complete(const QString& apiKey = QStringLiteral("test-key")) const
    {
        AnthropicClient client(apiKey, url());
        return client.createCompletion(testConfig(), pingMessages(), ToolsMap {});
    }

    void failWith(int status, const QString& errorType)
    {
        responseStatus = status;
        responseBody = QString(kErrorBody).arg(errorType).toUtf8();
    }
};

TEST_F(AnthropicTransportTest, TheRequest_CarriesTheKeyAndTheApiVersion)
{
    complete();

    EXPECT_TRUE(requestHasHeader("x-api-key: test-key"));
    EXPECT_TRUE(requestHasHeader("anthropic-version: 2026-10-21"));
    EXPECT_TRUE(requestHasHeader("content-type: application/json"));
    EXPECT_FALSE(requestHasHeader("authorization:"));
}

TEST_F(AnthropicTransportTest, TheRequest_PostsTheRequestBody)
{
    complete();

    EXPECT_EQ(requestJson(), anthropic::requestBody(testConfig(), pingMessages(), ToolsMap {}));
}

TEST_F(AnthropicTransportTest, AMessageReply_BecomesACompletionWithItsUsage)
{
    const Completion completion = complete();

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices[0].message.content, "mock reply");
    EXPECT_EQ(completion.usage.prompt_tokens, 33);
    EXPECT_EQ(completion.usage.completion_tokens, 5);
    EXPECT_EQ(completion.usage.context_tokens, 33);
    EXPECT_TRUE(completion.error.isEmpty());
}

TEST_F(AnthropicTransportTest, AnAuthenticationErrorBody_FailsWithTheKeyToken)
{
    failWith(401, "authentication_error");

    const Completion completion = complete();

    EXPECT_EQ(completion.error, "invalid_key");
    EXPECT_TRUE(completion.choices.isEmpty());
}

TEST_F(AnthropicTransportTest, AnOverloadedErrorBody_FailsWithTheServerToken)
{
    failWith(529, "overloaded_error");

    EXPECT_EQ(complete().error, "server_error");
}

TEST_F(AnthropicTransportTest, AnInvalidRequestBody_FailsWithTheUnknownToken)
{
    failWith(400, "invalid_request_error");

    EXPECT_EQ(complete().error, "unknown_error");
}

TEST_F(AnthropicTransportTest, AGatewayErrorWithoutAJsonBody_FallsBackToTheHttpStatus)
{
    responseStatus = 502;
    responseBody = "<html>Bad Gateway</html>";

    EXPECT_EQ(complete().error, "server_error");
}

TEST_F(AnthropicTransportTest, ASuccessfulReplyWithoutJson_Fails)
{
    responseBody = "not json";

    EXPECT_EQ(complete().error, "unknown_error");
}

TEST_F(AnthropicTransportTest, AnEmptyKey_FailsWithoutSendingARequest)
{
    const Completion completion = complete(QString());

    EXPECT_EQ(completion.error, "invalid_key");
    EXPECT_EQ(requestCount, 0);
}

TEST_F(AnthropicTransportTest, TheAsyncRequest_DeliversTheCompletionThroughTheCallback)
{
    AnthropicClient client("test-key", url());
    Completion delivered;
    bool finished = false;

    client.createCompletionAsync(
        testConfig(), pingMessages(), ToolsMap {},
        [&](Completion& completion)
        {
            delivered = completion;
            finished = true;
        },
        [](const QString&) {});

    QElapsedTimer timer;
    timer.start();
    while (!finished && timer.elapsed() < 2000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    ASSERT_TRUE(finished);
    ASSERT_EQ(delivered.choices.size(), 1);
    EXPECT_EQ(delivered.choices[0].message.content, "mock reply");
}
