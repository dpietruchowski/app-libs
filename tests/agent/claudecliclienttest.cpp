#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>

#include "agent/claudecliclient.h"

namespace
{
const char* kScript = R"(#!/bin/sh
cat > "$FAKE_CLAUDE_PROMPT"
printf '%s\036' "$@" > "$FAKE_CLAUDE_ARGS"
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
        argsPath = dir.path() + QStringLiteral("/args.txt");
        originalPath = qgetenv("PATH");
        qputenv("PATH", dir.path().toLocal8Bit() + ':' + originalPath);
        qputenv("FAKE_CLAUDE_PROMPT", promptPath.toLocal8Bit());
        qputenv("FAKE_CLAUDE_ARGS", argsPath.toLocal8Bit());
    }

    void TearDown() override
    {
        qputenv("PATH", originalPath);
        qunsetenv("FAKE_CLAUDE_OUTPUT");
        qunsetenv("FAKE_CLAUDE_PROMPT");
        qunsetenv("FAKE_CLAUDE_ARGS");
    }

    void answerWith(const QByteArray& output) { qputenv("FAKE_CLAUDE_OUTPUT", output); }

    static QString readAll(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return QString();
        return QString::fromUtf8(file.readAll());
    }

    QString prompt() const { return readAll(promptPath); }

    QStringList arguments() const
    {
        QStringList arguments = readAll(argsPath).split(QChar(0x1e));
        arguments.removeLast();
        return arguments;
    }

    QTemporaryDir dir;
    QString promptPath;
    QString argsPath;
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

TEST_F(ClaudeCliClientTest, TheConversationReachesTheBinaryOnStdin)
{
    answerWith(R"({"result":"ok"})");

    ClaudeCliClient().createCompletion(config, conversation(), {});

    const QString sent = prompt();
    EXPECT_TRUE(sent.contains(QStringLiteral("[user] make me a counter"))) << sent.toStdString();
    EXPECT_FALSE(sent.contains(QStringLiteral("be brief"))) << sent.toStdString();
}

TEST_F(ClaudeCliClientTest, EarlierToolCallsAreShownAsTheJsonTheModelEmitted)
{
    answerWith(R"({"result":"ok"})");
    Messages messages = conversation();
    Message call { .role = "assistant", .content = QString() };
    call.tool_calls.append(ToolCall { .id = "cli-create_app",
                                      .name = "create_app",
                                      .arguments = QVariantMap { { "id", "notes" } },
                                      .type = "function" });
    messages.append(call);
    messages.append(Message { .role = "tool", .content = "ok", .tool_call_id = "cli-create_app" });

    ClaudeCliClient().createCompletion(config, messages, {});

    const QString sent = prompt();
    EXPECT_TRUE(sent.contains(
        QStringLiteral("[assistant] {\"arguments\":{\"id\":\"notes\"},\"tool\":\"create_app\"}")))
        << sent.toStdString();
    EXPECT_TRUE(sent.contains(QStringLiteral("[tool result] ok"))) << sent.toStdString();
}

TEST_F(ClaudeCliClientTest, OurInstructionsReplaceTheSystemPromptOfTheBinary)
{
    answerWith(R"({"result":"ok"})");

    ClaudeCliClient().createCompletion(config, conversation(), {});

    const QStringList sent = arguments();
    const int systemPrompt = sent.indexOf(QStringLiteral("--system-prompt"));
    ASSERT_GE(systemPrompt, 0);
    ASSERT_LT(systemPrompt + 1, sent.size());
    EXPECT_TRUE(sent.at(systemPrompt + 1).contains(QStringLiteral("be brief")));
    EXPECT_FALSE(sent.at(systemPrompt + 1).contains(QStringLiteral("make me a counter")));
}

TEST_F(ClaudeCliClientTest, RunsHaikuWithNothingOfTheUsersSetup)
{
    answerWith(R"({"result":"ok"})");

    ClaudeCliClient().createCompletion(config, conversation(), {});

    const QStringList sent = arguments();
    const int model = sent.indexOf(QStringLiteral("--model"));
    ASSERT_GE(model, 0);
    EXPECT_EQ(sent.value(model + 1), QStringLiteral("haiku"));
    EXPECT_TRUE(sent.contains(QStringLiteral("--safe-mode")));
    EXPECT_TRUE(sent.contains(QStringLiteral("--no-session-persistence")));
    const int tools = sent.indexOf(QStringLiteral("--tools"));
    ASSERT_GE(tools, 0);
    ASSERT_LT(tools + 1, sent.size());
    EXPECT_TRUE(sent.at(tools + 1).isEmpty());
}

TEST_F(ClaudeCliClientTest, ToolsAreDescribedInTheSystemPrompt)
{
    answerWith(R"({"result":"ok"})");
    ToolsMap tools;
    tools.insert(QStringLiteral("create_app"),
                 ToolData { .tool = {},
                            .json = QJsonObject { { QStringLiteral("name"),
                                                    QStringLiteral("create_app") } } });

    ClaudeCliClient().createCompletion(config, conversation(), tools);

    const QStringList sent = arguments();
    const int systemPrompt = sent.indexOf(QStringLiteral("--system-prompt"));
    ASSERT_GE(systemPrompt, 0);
    EXPECT_TRUE(sent.value(systemPrompt + 1).contains(QStringLiteral("create_app")));
    EXPECT_FALSE(prompt().contains(QStringLiteral("create_app")));
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

TEST_F(ClaudeCliClientTest, AToolCallWrappedInProseTagsAndAListIsStillAToolCall)
{
    answerWith(R"({"result":"STATE: DESIGN\n\nid: notes\n\n**B1**\n<function_calls>\n[{\"tool\": \"create_app\", \"arguments\": {\"id\": \"notes\", \"qml\": \"Page { title: \\\"{a}\\\" }\"}}]\n</function_calls>"})");

    const Completion completion = ClaudeCliClient().createCompletion(config, conversation(), {});

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).finish_reason, "tool_calls");
    ASSERT_EQ(completion.choices.at(0).message.tool_calls.size(), 1);
    const ToolCall& call = completion.choices.at(0).message.tool_calls.at(0);
    EXPECT_EQ(call.name, "create_app");
    EXPECT_EQ(call.arguments.value("id").toString(), "notes");
    EXPECT_EQ(call.arguments.value("qml").toString(), "Page { title: \"{a}\" }");
}

TEST_F(ClaudeCliClientTest, SeveralCallsInAnthropicShapeAllBecomeToolCallsInOrder)
{
    answerWith(R"({"result":"<function_calls>\n[{\"function\": {\"name\": \"describe_app\", \"parameters\": {\"app\": \"notes\"}}}]\n</function_calls>\nNow the edit.\n<function_calls>\n[{\"function\": {\"name\": \"edit_file\", \"parameters\": {\"app\": \"notes\", \"path\": \"Data.qml\", \"before\": \"a\", \"after\": \"b\"}}}]\n</function_calls>\n<function_calls>\n[{\"function\": \"read_log\", \"parameters\": {\"app\": \"notes\"}}]\n</function_calls>\nGotowe."})");

    const Completion completion = ClaudeCliClient().createCompletion(config, conversation(), {});

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).finish_reason, "tool_calls");
    EXPECT_TRUE(completion.choices.at(0).message.content.isEmpty());
    const QVector<ToolCall>& calls = completion.choices.at(0).message.tool_calls;
    ASSERT_EQ(calls.size(), 3);
    EXPECT_EQ(calls.at(0).name, "describe_app");
    EXPECT_EQ(calls.at(0).arguments.value("app").toString(), "notes");
    EXPECT_EQ(calls.at(1).name, "edit_file");
    EXPECT_EQ(calls.at(1).arguments.value("after").toString(), "b");
    EXPECT_EQ(calls.at(2).name, "read_log");
    EXPECT_EQ(calls.at(2).arguments.value("app").toString(), "notes");
    EXPECT_NE(calls.at(0).id, calls.at(1).id);
}

TEST_F(ClaudeCliClientTest, ArgumentsPassedAsAJsonStringAreDecoded)
{
    answerWith(R"({"result":"{\"name\": \"read_file\", \"arguments\": \"{\\\"app\\\": \\\"notes\\\", \\\"path\\\": \\\"Main.qml\\\"}\"}"})");

    const Completion completion = ClaudeCliClient().createCompletion(config, conversation(), {});

    ASSERT_EQ(completion.choices.size(), 1);
    ASSERT_EQ(completion.choices.at(0).message.tool_calls.size(), 1);
    const ToolCall& call = completion.choices.at(0).message.tool_calls.at(0);
    EXPECT_EQ(call.name, "read_file");
    EXPECT_EQ(call.arguments.value("path").toString(), "Main.qml");
}

TEST_F(ClaudeCliClientTest, BracesInProseDoNotMakeAToolCall)
{
    answerWith(R"({"result":"Use { and } in QML, like Item { }."})");

    const Completion completion = ClaudeCliClient().createCompletion(config, conversation(), {});

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).finish_reason, "stop");
    EXPECT_EQ(completion.choices.at(0).message.content, "Use { and } in QML, like Item { }.");
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
