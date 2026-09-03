#include <gtest/gtest.h>

#include <vector>

#include <QVariantList>
#include <QVariantMap>

#include "agent/agentcontroller.h"
#include "agent/client.h"

namespace
{
class RecordingClient : public Client
{
public:
    RecordingClient()
        : Client("http://mock.invalid", "test-key")
    {
    }

    Completion createCompletion(const ModelConfig&, const Messages& messages, const ToolsMap&) const override
    {
        lastMessages = messages;
        return reply();
    }

    void createCompletionAsync(const ModelConfig&, const Messages& messages, const ToolsMap&,
                               const CompletionCreatedCallback& callback) const override
    {
        lastMessages = messages;
        Completion completion = reply();
        callback(completion);
    }

    mutable Messages lastMessages;

private:
    static Completion reply()
    {
        Completion completion;
        Choice choice;
        choice.index = 0;
        choice.finish_reason = "stop";
        choice.message = Message { .role = "assistant", .content = "ok" };
        completion.choices.append(choice);
        return completion;
    }
};

QVariantMap chatMessage(const QString& sender, const QString& text)
{
    return QVariantMap { { "sender", sender }, { "text", text } };
}
}

TEST(AgentControllerTest, Restore_ReplacesTheConversationWithTheGivenMessages)
{
    AgentController controller("gpt-test", "system prompt");
    RecordingClient client;
    controller.request(client, "first");

    controller.restore({ chatMessage("user", "hello"), chatMessage("assistant", "hi there") });

    const QVariantList messages = controller.messages();
    ASSERT_EQ(messages.size(), 2);
    EXPECT_EQ(messages[0].toMap().value("sender").toString(), "user");
    EXPECT_EQ(messages[0].toMap().value("text").toString(), "hello");
    EXPECT_EQ(messages[1].toMap().value("sender").toString(), "assistant");
    EXPECT_EQ(messages[1].toMap().value("text").toString(), "hi there");
    EXPECT_TRUE(controller.lastResponse().isEmpty());
}

TEST(AgentControllerTest, Restore_SendsTheRestoredMessagesToTheModelAfterTheSystemPrompt)
{
    AgentController controller("gpt-test", "system prompt");
    RecordingClient client;

    controller.restore({ chatMessage("user", "hello"), chatMessage("assistant", "hi there") });
    controller.request(client, "next");

    ASSERT_EQ(client.lastMessages.size(), 4);
    EXPECT_EQ(client.lastMessages[0].role, "system");
    EXPECT_EQ(client.lastMessages[0].content, "system prompt");
    EXPECT_EQ(client.lastMessages[1].content, "hello");
    EXPECT_EQ(client.lastMessages[2].content, "hi there");
    EXPECT_EQ(client.lastMessages[3].content, "next");
}

TEST(AgentControllerTest, Clear_WithoutAnInitialMessageLeavesOnlyTheSystemPrompt)
{
    AgentController controller("gpt-test", "system prompt");
    RecordingClient client;

    controller.clear("");
    controller.request(client, "next");

    ASSERT_EQ(client.lastMessages.size(), 2);
    EXPECT_EQ(client.lastMessages[0].role, "system");
    EXPECT_EQ(client.lastMessages[1].role, "user");
}
