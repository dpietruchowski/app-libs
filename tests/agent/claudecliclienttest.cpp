#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "agent/claudecliclient.h"

namespace
{
const char* kScript = R"(#!/bin/sh
cat > "$FAKE_CLAUDE_PROMPT"
printf '%s' "$FAKE_CLAUDE_OUTPUT"
)";

Messages conversation()
{
    return Messages { Message { .role = "system", .content = "be brief" },
                      Message { .role = "user", .content = "make me a counter" } };
}
}

class ClaudeCliClientTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const QString script = dir.path() + QStringLiteral("/claude");
        QFile file(script);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(kScript);
        file.close();
        ASSERT_TRUE(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        promptPath = dir.path() + QStringLiteral("/prompt.txt");
        originalPath = qgetenv("PATH");
        qputenv("PATH", dir.path().toLocal8Bit() + ':' + originalPath);
        qputenv("FAKE_CLAUDE_PROMPT", promptPath.toLocal8Bit());
    }

    void TearDown() override
    {
        qputenv("PATH", originalPath);
        qunsetenv("FAKE_CLAUDE_OUTPUT");
        qunsetenv("FAKE_CLAUDE_PROMPT");
    }

    void answerWith(const QByteArray& output) { qputenv("FAKE_CLAUDE_OUTPUT", output); }

    QString prompt() const
    {
        QFile file(promptPath);
        if (!file.open(QIODevice::ReadOnly))
            return QString();
        return QString::fromUtf8(file.readAll());
    }

    QTemporaryDir dir;
    QString promptPath;
    QByteArray originalPath;
    ModelConfig config { .model = "ignored", .reasoningEffort = "" };
};

TEST_F(ClaudeCliClientTest, FindsTheBinaryOnThePath)
{
    EXPECT_TRUE(ClaudeCliClient::isAvailable());
    EXPECT_EQ(ClaudeCliClient::executable(), dir.path() + QStringLiteral("/claude"));
}

TEST_F(ClaudeCliClientTest, PlainAnswerBecomesAnAssistantMessage)
{
    answerWith(R"({"type":"result","result":"Here you go."})");

    const Completion completion = ClaudeCliClient().createCompletion(config, conversation(), {});

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_TRUE(completion.error.isEmpty());
    EXPECT_EQ(completion.choices.at(0).finish_reason, "stop");
    EXPECT_EQ(completion.choices.at(0).message.content, "Here you go.");
}

TEST_F(ClaudeCliClientTest, TheConversationReachesTheBinary)
{
    answerWith(R"({"result":"ok"})");

    ClaudeCliClient().createCompletion(config, conversation(), {});

    const QString sent = prompt();
    EXPECT_TRUE(sent.contains(QStringLiteral("be brief"))) << sent.toStdString();
    EXPECT_TRUE(sent.contains(QStringLiteral("[user] make me a counter"))) << sent.toStdString();
}

TEST_F(ClaudeCliClientTest, AJsonAnswerBecomesAToolCall)
{
    answerWith(R"({"result":"{\"tool\":\"create_app\",\"arguments\":{\"id\":\"notes\"}}"})");

    const Completion completion = ClaudeCliClient().createCompletion(config, conversation(), {});

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).finish_reason, "tool_calls");
    ASSERT_EQ(completion.choices.at(0).message.tool_calls.size(), 1);

    const ToolCall& call = completion.choices.at(0).message.tool_calls.at(0);
    EXPECT_EQ(call.name, "create_app");
    EXPECT_EQ(call.type, "function");
    EXPECT_FALSE(call.id.isEmpty());
    EXPECT_EQ(call.arguments.value("id").toString(), "notes");
}

TEST_F(ClaudeCliClientTest, AFencedJsonAnswerIsStillAToolCall)
{
    answerWith(R"({"result":"```json\n{\"tool\":\"create_app\",\"arguments\":{}}\n```"})");

    const Completion completion = ClaudeCliClient().createCompletion(config, conversation(), {});

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).finish_reason, "tool_calls");
}

TEST_F(ClaudeCliClientTest, AnEmptyAnswerIsAnError)
{
    answerWith(R"({"result":""})");

    const Completion completion = ClaudeCliClient().createCompletion(config, conversation(), {});

    EXPECT_TRUE(completion.choices.isEmpty());
    EXPECT_FALSE(completion.error.isEmpty());
}

TEST_F(ClaudeCliClientTest, AMissingBinaryIsAnError)
{
    qputenv("PATH", QByteArray());

    const Completion completion = ClaudeCliClient().createCompletion(config, conversation(), {});

    EXPECT_TRUE(completion.choices.isEmpty());
    EXPECT_EQ(completion.error, "network_error");
}
