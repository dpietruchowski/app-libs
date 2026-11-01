#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "agent/claudecli.h"
#include "environmentoverride.h"

namespace
{

ModelConfig configOf(const QString& model, const QString& effort = {}) { return { model, effort }; }

Message messageOf(const QString& role, const QString& content)
{
    Message message;
    message.role = role;
    message.content = content;
    return message;
}

}

TEST(ClaudeCliTest, ExposesTheCategoryTokensOfAFailedCompletion)
{
    EXPECT_EQ(claudecli::kNetworkError, "network_error");
    EXPECT_EQ(claudecli::kServerError, "server_error");
    EXPECT_EQ(claudecli::kTimeout, "timeout");
    EXPECT_EQ(claudecli::kNotLoggedIn, "not_logged_in");
}

TEST(ClaudeCliTest, AcceptsASessionIdInTheCanonicalUuidForm)
{
    EXPECT_TRUE(claudecli::isSessionId("64174da0-2cdb-4392-b7ec-5fe0b2f28b20"));
    EXPECT_TRUE(claudecli::isSessionId("64174DA0-2CDB-4392-B7EC-5FE0B2F28B20"));
}

TEST(ClaudeCliTest, RejectsAnythingThatIsNotAUuidAsASessionId)
{
    for (const QString& id : { QString(), QString("abc"), QString("--resume"),
                               QString("{64174da0-2cdb-4392-b7ec-5fe0b2f28b20}"),
                               QString("64174da02cdb4392b7ec5fe0b2f28b20"),
                               QString("64174da0-2cdb-4392-b7ec-5fe0b2f28b2"),
                               QString("64174da0-2cdb-4392-b7ec-5fe0b2f28b200"),
                               QString("64174da0-2cdb-4392-b7ec-5fe0b2f28b2g"),
                               QString("64174da0-2cdb-4392-b7ec-5fe0b2f28b20\n"),
                               QString(" 64174da0-2cdb-4392-b7ec-5fe0b2f28b20") })
        EXPECT_FALSE(claudecli::isSessionId(id)) << id.toStdString();
}

TEST(ClaudeCliTest, BuildsAFailedCompletionWithoutChoices)
{
    const Completion completion = claudecli::failure(claudecli::kTimeout);

    EXPECT_EQ(completion.error, "timeout");
    EXPECT_TRUE(completion.choices.isEmpty());
}

TEST(ClaudeCliTest, FindsTheClaudeExecutableOnThePath)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString program = QDir(directory.path()).filePath("claude");
    QFile file(program);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\n");
    file.close();
    ASSERT_TRUE(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner));
    const EnvironmentOverride path("PATH", directory.path().toUtf8());

    EXPECT_EQ(QFileInfo(claudecli::executable()), QFileInfo(program));
}

TEST(ClaudeCliTest, ReportsNoExecutableWhenThePathHasNone)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const EnvironmentOverride path("PATH", directory.path().toUtf8());

    EXPECT_TRUE(claudecli::executable().isEmpty());
}

TEST(ClaudeCliTest, PassesAKnownModelAliasThroughInLowerCase)
{
    for (const QString& alias :
         { "haiku", "sonnet", "opus", "fable", "default", "sonnet[1m]", "opus[1m]" })
        EXPECT_EQ(claudecli::modelOf(configOf(alias)), alias) << alias.toStdString();
    EXPECT_EQ(claudecli::modelOf(configOf("OPUS")), "opus");
}

TEST(ClaudeCliTest, PassesAFullClaudeModelNameThrough)
{
    EXPECT_EQ(claudecli::modelOf(configOf("claude-sonnet-5-5")), "claude-sonnet-5-5");
    EXPECT_EQ(claudecli::modelOf(configOf("Claude-Opus-5-5")), "claude-opus-5-5");
}

TEST(ClaudeCliTest, FallsBackToHaikuForAModelTheCliDoesNotKnow)
{
    EXPECT_EQ(claudecli::modelOf(configOf("gpt-4o")), "haiku");
    EXPECT_EQ(claudecli::modelOf(configOf("")), "haiku");
}

TEST(ClaudeCliTest, PassesAKnownEffortLevelThroughInLowerCase)
{
    for (const QString& level : { "low", "medium", "high", "xhigh", "max" })
        EXPECT_EQ(claudecli::effortOf(configOf("haiku", level)), level) << level.toStdString();
    EXPECT_EQ(claudecli::effortOf(configOf("haiku", "HIGH")), "high");
}

TEST(ClaudeCliTest, DropsAnUnknownOrEmptyEffort)
{
    EXPECT_TRUE(claudecli::effortOf(configOf("haiku", "extreme")).isEmpty());
    EXPECT_TRUE(claudecli::effortOf(configOf("haiku", "")).isEmpty());
}

TEST(ClaudeCliTest, JoinsTheSystemMessagesEachFollowedByABlankLine)
{
    const Messages messages { messageOf("system", "Be brief."), messageOf("user", "Hi"),
                              messageOf("system", "Answer in Polish.") };

    EXPECT_EQ(claudecli::systemMessages(messages), "Be brief.\n\nAnswer in Polish.\n\n");
}

TEST(ClaudeCliTest, GivesNoSystemTextWithoutSystemMessages)
{
    EXPECT_TRUE(claudecli::systemMessages({ messageOf("user", "Hi") }).isEmpty());
    EXPECT_TRUE(claudecli::systemMessages({}).isEmpty());
}

TEST(ClaudeCliTest, LabelsEveryMessageOfTheConversationByRole)
{
    const Messages messages { messageOf("user", "Hello"), messageOf("assistant", "Hi there"),
                              messageOf("tool", "42") };

    EXPECT_EQ(claudecli::conversation(messages),
              "Conversation so far:\n[user] Hello\n[assistant] Hi there\n[tool result] 42\n");
}

TEST(ClaudeCliTest, LeavesTheSystemMessagesOutOfTheConversation)
{
    const Messages messages { messageOf("system", "Be brief."), messageOf("user", "Hello") };

    EXPECT_EQ(claudecli::conversation(messages), "Conversation so far:\n[user] Hello\n");
}

TEST(ClaudeCliTest, WritesTheToolCallsOfAnAssistantMessageAsJson)
{
    Message assistant = messageOf("assistant", "ignored text");
    ToolCall call;
    call.id = "call-1";
    call.name = "lookup";
    call.arguments = { { "id", 7 } };
    assistant.tool_calls.append(call);
    ToolCall second;
    second.id = "call-2";
    second.name = "ping";
    assistant.tool_calls.append(second);

    EXPECT_EQ(claudecli::conversation({ assistant }),
              "Conversation so far:\n[assistant] "
              "{\"arguments\":{\"id\":7},\"tool\":\"lookup\"}\n"
              "{\"arguments\":{},\"tool\":\"ping\"}\n");
}

TEST(ClaudeCliTest, GivesOnlyTheHeaderForAnEmptyConversation)
{
    EXPECT_EQ(claudecli::conversation({}), "Conversation so far:\n");
}
