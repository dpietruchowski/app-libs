#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include "agent/structures.h"

class StructuresTest : public ::testing::Test
{
};

TEST_F(StructuresTest, Message_RoundTripsRoleAndContent)
{
    Message original { .role = "user", .content = "hello world" };

    Message restored = Message::fromJsonObject(original.toJsonObject());

    EXPECT_EQ(restored.role, "user");
    EXPECT_EQ(restored.content, "hello world");
}

TEST_F(StructuresTest, Message_RoundTripsToolCalls)
{
    ToolCall call;
    call.id = "call_1";
    call.name = "add_numbers";
    call.type = "function";
    call.arguments = QVariantMap { { "a", 2 }, { "b", 3 } };

    Message original { .role = "assistant", .content = "" };
    original.tool_calls.append(call);

    Message restored = Message::fromJsonObject(original.toJsonObject());

    ASSERT_EQ(restored.tool_calls.size(), 1);
    EXPECT_EQ(restored.tool_calls[0].id, "call_1");
    EXPECT_EQ(restored.tool_calls[0].name, "add_numbers");
    EXPECT_EQ(restored.tool_calls[0].type, "function");
    EXPECT_EQ(restored.tool_calls[0].arguments.value("a").toInt(), 2);
    EXPECT_EQ(restored.tool_calls[0].arguments.value("b").toInt(), 3);
}

TEST_F(StructuresTest, ToolCall_SerializesArgumentsAsJsonString)
{
    ToolCall call;
    call.id = "id";
    call.name = "fn";
    call.type = "function";
    call.arguments = QVariantMap { { "key", "value" } };

    QJsonObject json = call.toJsonObject();

    // The OpenAI schema requires function.arguments to be a JSON *string*.
    ASSERT_TRUE(json["function"].toObject()["arguments"].isString());
    QString args = json["function"].toObject()["arguments"].toString();
    EXPECT_TRUE(args.contains("\"key\""));
    EXPECT_TRUE(args.contains("\"value\""));
}

TEST_F(StructuresTest, Usage_RoundTrips)
{
    Usage original { .prompt_tokens = 11, .completion_tokens = 22, .total_tokens = 33 };

    Usage restored = Usage::fromJsonObject(original.toJsonObject());

    EXPECT_EQ(restored.prompt_tokens, 11);
    EXPECT_EQ(restored.completion_tokens, 22);
    EXPECT_EQ(restored.total_tokens, 33);
}

TEST_F(StructuresTest, Completion_RoundTripsFullObject)
{
    Completion original;
    original.id = "cmpl-1";
    original.object = "chat.completion";
    original.created = 1700000000;
    original.model = "gpt-test";
    original.usage = Usage { .prompt_tokens = 1, .completion_tokens = 2, .total_tokens = 3 };

    Choice choice;
    choice.index = 0;
    choice.finish_reason = "stop";
    choice.message = Message { .role = "assistant", .content = "the answer" };
    original.choices.append(choice);

    Completion restored = Completion::fromJsonObject(original.toJsonObject());

    EXPECT_EQ(restored.id, "cmpl-1");
    EXPECT_EQ(restored.object, "chat.completion");
    EXPECT_EQ(restored.created, 1700000000);
    EXPECT_EQ(restored.model, "gpt-test");
    EXPECT_EQ(restored.usage.total_tokens, 3);
    ASSERT_EQ(restored.choices.size(), 1);
    EXPECT_EQ(restored.choices[0].finish_reason, "stop");
    EXPECT_EQ(restored.choices[0].message.content, "the answer");
}

TEST_F(StructuresTest, Messages_RoundTripThroughJsonArray)
{
    Messages messages;
    messages.append(Message { .role = "system", .content = "be brief" });
    messages.append(Message { .role = "user", .content = "hi" });

    Messages restored = fromJsonArray(toJsonArray(messages));

    ASSERT_EQ(restored.size(), 2);
    EXPECT_EQ(restored[0].role, "system");
    EXPECT_EQ(restored[1].content, "hi");
}

TEST_F(StructuresTest, ToolsMap_SerializesToJsonArrayOfSchemas)
{
    QJsonObject schema;
    schema["type"] = "function";

    ToolsMap tools;
    tools.insert("fn", ToolData { .tool = {}, .json = schema });

    QJsonArray array = toJsonArray(tools);

    ASSERT_EQ(array.size(), 1);
    EXPECT_EQ(array[0].toObject()["type"].toString(), "function");
}
