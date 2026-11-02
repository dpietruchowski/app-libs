#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include "agent/anthropicclient.h"

namespace
{
const QString kToolContent = QStringLiteral("{\"value\":\"Shot\"}");

QJsonObject messageResponse(const QString& stopReason)
{
    QJsonObject response {
        { "id", "msg_1" },
        { "type", "message" },
        { "role", "assistant" },
        { "content", QJsonArray { QJsonObject { { "type", "text" }, { "text", "hi" } } } }
    };
    if (!stopReason.isEmpty())
    {
        response["stop_reason"] = stopReason;
    }
    return response;
}

Message screenshotResult()
{
    Message message { .role = "tool", .content = kToolContent, .tool_call_id = "call_1" };
    message.images.append(
        ToolImage { .text = "Shot", .mimeType = "image/png", .data = "png-bytes" });
    return message;
}

QJsonArray toolResultContent(const Messages& messages)
{
    const QJsonObject body
        = anthropic::requestBody(ModelConfig { .model = "claude-test" }, messages, ToolsMap {});
    return body.value("messages")
        .toArray()
        .last()
        .toObject()
        .value("content")
        .toArray()
        .first()
        .toObject()
        .value("content")
        .toArray();
}
}

TEST(AnthropicEdgeCaseTest, AnEmptyToolsMap_OmitsTheToolsKey)
{
    const QJsonObject body
        = anthropic::requestBody(ModelConfig { .model = "claude-test" },
                                 { Message { .role = "user", .content = "hi" } }, ToolsMap {});

    EXPECT_FALSE(body.contains("tools"));
}

TEST(AnthropicEdgeCaseTest, AToolResultWithAnImage_CarriesTheTextAndTheImageBlock)
{
    const QJsonArray content = toolResultContent({ screenshotResult() });

    ASSERT_EQ(content.size(), 2);
    EXPECT_EQ(content[0].toObject().value("type").toString(), "text");
    EXPECT_EQ(content[0].toObject().value("text").toString(), kToolContent);
    const QJsonObject image = content[1].toObject();
    const QJsonObject source = image.value("source").toObject();
    EXPECT_EQ(image.value("type").toString(), "image");
    EXPECT_EQ(source.value("type").toString(), "base64");
    EXPECT_EQ(source.value("media_type").toString(), "image/png");
    EXPECT_EQ(source.value("data").toString(),
              QString::fromLatin1(QByteArray("png-bytes").toBase64()));
}

TEST(AnthropicEdgeCaseTest, AToolResultWithTwoImages_KeepsThemInOrderAfterTheText)
{
    Message message = screenshotResult();
    message.images.append(
        ToolImage { .text = "Second", .mimeType = "image/jpeg", .data = "jpeg-bytes" });

    const QJsonArray content = toolResultContent({ message });

    ASSERT_EQ(content.size(), 3);
    EXPECT_EQ(content[1].toObject().value("source").toObject().value("media_type").toString(),
              "image/png");
    EXPECT_EQ(content[2].toObject().value("source").toObject().value("media_type").toString(),
              "image/jpeg");
}

TEST(AnthropicEdgeCaseTest, APauseTurn_FailsWithTheUnknownToken)
{
    const Completion completion = anthropic::completionFrom(messageResponse("pause_turn"));

    EXPECT_EQ(completion.error, "unknown_error");
    EXPECT_TRUE(completion.choices.isEmpty());
}

TEST(AnthropicEdgeCaseTest, AnUnknownStopReason_FailsWithTheUnknownToken)
{
    const Completion completion = anthropic::completionFrom(messageResponse("something_new"));

    EXPECT_EQ(completion.error, "unknown_error");
    EXPECT_TRUE(completion.choices.isEmpty());
}

TEST(AnthropicEdgeCaseTest, AMissingStopReason_FailsWithTheUnknownToken)
{
    const Completion completion = anthropic::completionFrom(messageResponse(QString()));

    EXPECT_EQ(completion.error, "unknown_error");
    EXPECT_TRUE(completion.choices.isEmpty());
}

TEST(AnthropicEdgeCaseTest, AnEmptyBody_FailsWithTheUnknownToken)
{
    EXPECT_EQ(anthropic::completionFrom(QJsonObject {}).error, "unknown_error");
}

TEST(AnthropicEdgeCaseTest, TheRawContentIsNotPartOfTheOpenAiMessageJson)
{
    Message message { .role = "assistant", .content = "hi" };
    message.raw_content = QJsonArray { QJsonObject { { "type", "thinking" } } };

    EXPECT_FALSE(message.toJsonObject().contains("raw_content"));
}
