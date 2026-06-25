#include <gtest/gtest.h>

#include <vector>

#include "agent/agent.h"
#include "agent/client.h"

namespace
{
// A Client that returns scripted completions instead of hitting the network.
// Each call consumes the next completion in the script.
class MockClient : public Client
{
public:
    MockClient()
        : Client("http://mock.invalid", "test-key")
    {
    }

    std::vector<Completion> script;
    mutable int calls = 0;
    mutable Messages lastMessages;

    Completion createCompletion(const ModelConfig&, const Messages& messages, const ToolsMap&) const override
    {
        lastMessages = messages;
        return script.at(calls++);
    }

    void createCompletionAsync(const ModelConfig&, const Messages& messages, const ToolsMap&,
                               const CompletionCreatedCallback& callback) const override
    {
        lastMessages = messages;
        Completion completion = script.at(calls++);
        callback(completion);
    }
};

Completion contentCompletion(const QString& text)
{
    Completion completion;
    Choice choice;
    choice.index = 0;
    choice.finish_reason = "stop";
    choice.message = Message { .role = "assistant", .content = text };
    completion.choices.append(choice);
    return completion;
}

Completion toolCallCompletion(const QString& id, const QString& name, const QVariantMap& arguments)
{
    Completion completion;
    Choice choice;
    choice.index = 0;
    choice.finish_reason = "tool_calls";
    Message message { .role = "assistant", .content = "" };
    ToolCall call;
    call.id = id;
    call.name = name;
    call.type = "function";
    call.arguments = arguments;
    message.tool_calls.append(call);
    choice.message = message;
    completion.choices.append(choice);
    return completion;
}

// Docstring in the @brief/@details/@param format the agent parses into a schema.
const QString kAddToolDocstring = R"(@brief add_numbers
@details Adds two numbers together
@param a (number) the first number
@param b (number) the second number
@return the sum)";

QString requestSync(Agent& agent, const Client& client, const QString& message)
{
    QString delivered;
    agent.requestAsync(client, message, [&delivered](const QString& response) { delivered = response; });
    return delivered;
}
}

class AgentTest : public ::testing::Test
{
};

TEST_F(AgentTest, Request_WithoutTools_ReturnsAssistantContent)
{
    MockClient client;
    client.script = { contentCompletion("hi there") };

    Agent agent("gpt-test", "You are a coach.");
    QString response = requestSync(agent, client, "hello");

    EXPECT_EQ(response, "hi there");
    EXPECT_EQ(client.calls, 1);
}

TEST_F(AgentTest, Request_BuildsSystemUserAssistantMessageSequence)
{
    MockClient client;
    client.script = { contentCompletion("reply") };

    Agent agent("gpt-test", "system prompt");
    requestSync(agent, client, "user says hi");

    const Messages& messages = agent.messages();
    ASSERT_EQ(messages.size(), 3);
    EXPECT_EQ(messages[0].role, "system");
    EXPECT_EQ(messages[0].content, "system prompt");
    EXPECT_EQ(messages[1].role, "user");
    EXPECT_EQ(messages[1].content, "user says hi");
    EXPECT_EQ(messages[2].role, "assistant");
    EXPECT_EQ(messages[2].content, "reply");
}

TEST_F(AgentTest, Request_WithToolCall_InvokesToolThenReturnsFinalContent)
{
    MockClient client;
    client.script = {
        toolCallCompletion("call_1", "add_numbers", QVariantMap { { "a", 2 }, { "b", 3 } }),
        contentCompletion("the sum is 5"),
    };

    Agent agent("gpt-test", "system");

    bool toolInvoked = false;
    QVariantMap receivedArgs;
    agent.addTool(kAddToolDocstring,
                  [&](QVariantMap args, ToolDone done)
                  {
                      toolInvoked = true;
                      receivedArgs = args;
                      done(args.value("a").toInt() + args.value("b").toInt());
                  });

    QString response = requestSync(agent, client, "add 2 and 3");

    EXPECT_TRUE(toolInvoked);
    EXPECT_EQ(receivedArgs.value("a").toInt(), 2);
    EXPECT_EQ(receivedArgs.value("b").toInt(), 3);
    EXPECT_EQ(response, "the sum is 5");
    EXPECT_EQ(client.calls, 2);

    // system, user, assistant(tool_calls), tool result, assistant(final)
    const Messages& messages = agent.messages();
    ASSERT_EQ(messages.size(), 5);
    EXPECT_EQ(messages[3].role, "tool");
    EXPECT_EQ(messages[3].tool_call_id, "call_1");
    EXPECT_EQ(messages[4].content, "the sum is 5");
}

TEST_F(AgentTest, RequestAsync_DeliversResponseThroughCallback)
{
    MockClient client;
    client.script = { contentCompletion("async reply") };

    Agent agent("gpt-test", "system");

    QString delivered;
    agent.requestAsync(client, "hello", [&](const QString& response) { delivered = response; });

    EXPECT_EQ(delivered, "async reply");
    EXPECT_FALSE(agent.asynRequestInProgress());
}

TEST_F(AgentTest, Clear_ResetsToSystemMessageOnly)
{
    MockClient client;
    client.script = { contentCompletion("reply") };

    Agent agent("gpt-test", "the system prompt");
    requestSync(agent, client, "hi");
    ASSERT_GT(agent.messages().size(), 1);

    agent.clear();

    ASSERT_EQ(agent.messages().size(), 1);
    EXPECT_EQ(agent.messages()[0].role, "system");
    EXPECT_EQ(agent.messages()[0].content, "the system prompt");
}

TEST_F(AgentTest, UnknownToolName_IsSkippedAndConversationContinues)
{
    MockClient client;
    client.script = {
        toolCallCompletion("call_x", "missing_tool", QVariantMap {}),
        contentCompletion("done anyway"),
    };

    Agent agent("gpt-test", "system");
    QString response = requestSync(agent, client, "go");

    EXPECT_EQ(response, "done anyway");
    EXPECT_EQ(client.calls, 2);
}
