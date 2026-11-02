#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "agent/anthropicclient.h"
#include "agent/structures.h"
#include "completions.h"
#include "toolsmapof.h"

namespace
{

const char* kTextResponse = R"json({
  "id": "msg_01Aq9w938a90dw8q",
  "type": "message",
  "role": "assistant",
  "model": "claude-opus-5-5",
  "content": [{"type": "text", "text": "The capital of France is Paris."}],
  "stop_reason": "end_turn",
  "stop_sequence": null,
  "usage": {"input_tokens": 14, "output_tokens": 9,
            "cache_creation_input_tokens": 0, "cache_read_input_tokens": 0}
})json";

const char* kToolUseResponse = R"json({
  "id": "msg_01Bq2x7w5k1p0d3n",
  "type": "message",
  "role": "assistant",
  "model": "claude-opus-5-5",
  "content": [
    {"type": "text", "text": "Let me check the weather."},
    {"type": "tool_use", "id": "toolu_abc123", "name": "get_weather",
     "input": {"location": "Paris"}}
  ],
  "stop_reason": "tool_use",
  "stop_sequence": null,
  "usage": {"input_tokens": 21, "output_tokens": 38,
            "cache_creation_input_tokens": 0, "cache_read_input_tokens": 0}
})json";

const char* kThinkingToolUseResponse = R"json({
  "id": "msg_01Cr3y8x6l2q1e4o",
  "type": "message",
  "role": "assistant",
  "model": "claude-opus-5-5",
  "content": [
    {"type": "thinking", "thinking": "The user wants the weather, so I should call the tool.",
     "signature": "EuYBCkQYAiJAbm9uLWNyeXB0b2dyYXBoaWMtc2lnbmF0dXJl"},
    {"type": "redacted_thinking", "data": "EmwKAhgBEgy3va3pzix/LafPsn4aDFIT2Xlxh0L5L8rLVyIw"},
    {"type": "text", "text": "Let me check the weather."},
    {"type": "tool_use", "id": "toolu_abc123", "name": "get_weather",
     "input": {"location": "Paris"}}
  ],
  "stop_reason": "tool_use",
  "stop_sequence": null,
  "usage": {"input_tokens": 21, "output_tokens": 112,
            "cache_creation_input_tokens": 0, "cache_read_input_tokens": 0}
})json";

const char* kCachedUsageResponse = R"json({
  "id": "msg_01Ds4z9y7m3r2f5p",
  "type": "message",
  "role": "assistant",
  "model": "claude-opus-5-5",
  "content": [{"type": "text", "text": "Still Paris."}],
  "stop_reason": "end_turn",
  "stop_sequence": null,
  "usage": {"input_tokens": 21, "output_tokens": 50,
            "cache_creation_input_tokens": 1000, "cache_read_input_tokens": 5000,
            "cache_creation": {"ephemeral_5m_input_tokens": 0, "ephemeral_1h_input_tokens": 1000}}
})json";

const char* kRawAssistantContent = R"json([
  {"type": "thinking", "thinking": "The user wants the weather, so I should call the tool.",
   "signature": "EuYBCkQYAiJAbm9uLWNyeXB0b2dyYXBoaWMtc2lnbmF0dXJl"},
  {"type": "redacted_thinking", "data": "EmwKAhgBEgy3va3pzix/LafPsn4aDFIT2Xlxh0L5L8rLVyIw"},
  {"type": "text", "text": "Let me check the weather."},
  {"type": "tool_use", "id": "toolu_abc123", "name": "get_weather", "input": {"location": "Paris"}}
])json";

QJsonDocument parse(const char* text)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(text), &error);
    EXPECT_EQ(error.error, QJsonParseError::NoError) << text;
    return doc;
}

QJsonObject parseObject(const char* text) { return parse(text).object(); }

QJsonArray parseArray(const char* text) { return parse(text).array(); }

template <typename Json> QByteArray compact(const Json& json)
{
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

template <typename Json>
::testing::AssertionResult sameJson(const Json& actual, const Json& expected)
{
    if (actual == expected)
    {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "\nactual:   " << compact(actual).constData()
                                         << "\nexpected: " << compact(expected).constData();
}

Message assistantCalling(const QString& text, const QVector<ToolCall>& calls)
{
    Message message { .role = "assistant", .content = text };
    message.tool_calls = calls;
    return message;
}

ToolData weatherTool()
{
    const QJsonObject function { { "strict", true },
                                 { "name", "get_weather" },
                                 { "description", "Get current weather for a location" },
                                 { "parameters", parseObject(R"json({
            "type": "object",
            "properties": {"location": {"type": "string", "description": "City name"}},
            "required": ["location"]
          })json") } };
    return ToolData { .tool = nullptr,
                      .json = QJsonObject { { "type", "function" }, { "function", function } } };
}

QJsonObject cacheControlOneHour()
{
    return parseObject(R"json({"type": "ephemeral", "ttl": "1h"})json");
}

QStringList blockTypes(const QJsonArray& blocks)
{
    QStringList types;
    for (const QJsonValue& block : blocks)
    {
        types.append(block.toObject().value("type").toString());
    }
    return types;
}

}

class AnthropicRequestTest : public ::testing::Test
{
protected:
    static QJsonObject build(const Messages& messages, const ToolsMap& tools = {},
                             const QString& effort = {})
    {
        return anthropic::requestBody(
            ModelConfig { .model = "claude-opus-5-5", .reasoningEffort = effort }, messages, tools);
    }

    static QJsonArray sentMessages(const QJsonObject& body)
    {
        return body.value("messages").toArray();
    }

    static QJsonObject sentMessage(const QJsonObject& body, int index)
    {
        return sentMessages(body).at(index).toObject();
    }

    static QString effortFor(const QString& configured)
    {
        const QJsonObject body
            = build({ Message { .role = "user", .content = "Hi" } }, {}, configured);
        return body.value("output_config").toObject().value("effort").toString();
    }

    static Messages weatherConversation()
    {
        return { Message { .role = "system", .content = "You are a weather assistant." },
                 Message { .role = "user", .content = "What is the weather in Paris?" },
                 assistantCalling("Let me check the weather.",
                                  { agent_test::toolCall("toolu_abc123", "get_weather",
                                                         { { "location", "Paris" } }) }),
                 Message { .role = "tool",
                           .content = QString::fromUtf8("72°F and sunny"),
                           .tool_call_id = "toolu_abc123" } };
    }

    static ToolsMap weatherTools() { return ToolsMap { { "get_weather", weatherTool() } }; }
};

TEST_F(AnthropicRequestTest, Model_ComesFromTheConfig)
{
    const QJsonObject body
        = anthropic::requestBody(ModelConfig { .model = "claude-haiku-5-5" },
                                 { Message { .role = "user", .content = "Hi" } }, {});

    EXPECT_EQ(body.value("model").toString(), "claude-haiku-5-5");
}

TEST_F(AnthropicRequestTest, MaxTokens_Is16000)
{
    const QJsonObject body = build({ Message { .role = "user", .content = "Hi" } });

    EXPECT_EQ(body.value("max_tokens").toInt(), 16000);
}

TEST_F(AnthropicRequestTest, System_BecomesTopLevelTextBlocks)
{
    const QJsonObject body = build({ Message { .role = "system", .content = "Be brief." },
                                     Message { .role = "system", .content = "Answer in Polish." },
                                     Message { .role = "user", .content = "Hi" } });

    const QJsonArray system = body.value("system").toArray();
    ASSERT_EQ(system.size(), 2);
    EXPECT_EQ(system.at(0).toObject().value("type").toString(), "text");
    EXPECT_EQ(system.at(0).toObject().value("text").toString(), "Be brief.");
    EXPECT_EQ(system.at(1).toObject().value("type").toString(), "text");
    EXPECT_EQ(system.at(1).toObject().value("text").toString(), "Answer in Polish.");
}

TEST_F(AnthropicRequestTest, System_SingleMessageMatchesTheDocumentedCachedBlock)
{
    const QJsonObject body
        = build({ Message { .role = "system", .content = "You are a weather assistant." },
                  Message { .role = "user", .content = "Hi" } });

    EXPECT_TRUE(sameJson(body.value("system").toArray(),
                         parseArray(R"json([{"type": "text", "text": "You are a weather assistant.",
                                             "cache_control": {"type": "ephemeral", "ttl": "1h"}}])json")));
}

TEST_F(AnthropicRequestTest, System_LastBlockCarriesOneHourCacheControl)
{
    const QJsonObject body = build({ Message { .role = "system", .content = "Be brief." },
                                     Message { .role = "system", .content = "Answer in Polish." },
                                     Message { .role = "user", .content = "Hi" } });

    const QJsonArray system = body.value("system").toArray();
    ASSERT_EQ(system.size(), 2);
    EXPECT_TRUE(
        sameJson(system.at(1).toObject().value("cache_control").toObject(), cacheControlOneHour()));
}

TEST_F(AnthropicRequestTest, System_OnlyTheLastBlockCarriesCacheControl)
{
    const QJsonObject body = build({ Message { .role = "system", .content = "Be brief." },
                                     Message { .role = "system", .content = "Answer in Polish." },
                                     Message { .role = "system", .content = "Never apologise." },
                                     Message { .role = "user", .content = "Hi" } });

    const QJsonArray system = body.value("system").toArray();
    ASSERT_EQ(system.size(), 3);
    EXPECT_FALSE(system.at(0).toObject().contains("cache_control"));
    EXPECT_FALSE(system.at(1).toObject().contains("cache_control"));
    EXPECT_TRUE(system.at(2).toObject().contains("cache_control"));
}

TEST_F(AnthropicRequestTest, System_MessagesAreNotSentAsConversationMessages)
{
    const QJsonObject body = build(weatherConversation());

    for (const QJsonValue& sent : sentMessages(body))
    {
        EXPECT_NE(sent.toObject().value("role").toString(), "system");
    }
    EXPECT_EQ(sentMessages(body).size(), 3);
}

TEST_F(AnthropicRequestTest, System_WithoutSystemMessagesIsOmitted)
{
    const QJsonObject body = build({ Message { .role = "user", .content = "Hi" } });

    EXPECT_FALSE(body.contains("system"));
}

TEST_F(AnthropicRequestTest, TopLevelCacheControl_IsEphemeralOneHour)
{
    const QJsonObject body = build(weatherConversation(), weatherTools());

    EXPECT_TRUE(sameJson(body.value("cache_control").toObject(), cacheControlOneHour()));
}

TEST_F(AnthropicRequestTest, TopLevelCacheControl_IsPresentWithoutSystemMessages)
{
    const QJsonObject body = build({ Message { .role = "user", .content = "Hi" } });

    EXPECT_TRUE(sameJson(body.value("cache_control").toObject(), cacheControlOneHour()));
}

TEST_F(AnthropicRequestTest, Tools_MapToNameDescriptionAndInputSchema)
{
    const QJsonObject body = build(weatherConversation(), weatherTools());

    EXPECT_TRUE(sameJson(body.value("tools").toArray(), parseArray(R"json([{
        "name": "get_weather",
        "description": "Get current weather for a location",
        "input_schema": {
          "type": "object",
          "properties": {"location": {"type": "string", "description": "City name"}},
          "required": ["location"]
        }
      }])json")));
}

TEST_F(AnthropicRequestTest, Tools_DropTheOpenAiWrapperAndStrictFlag)
{
    const QJsonObject body = build(weatherConversation(), weatherTools());

    const QJsonObject tool = body.value("tools").toArray().at(0).toObject();
    EXPECT_FALSE(tool.contains("type"));
    EXPECT_FALSE(tool.contains("function"));
    EXPECT_FALSE(tool.contains("strict"));
    EXPECT_EQ(tool.keys().size(), 3);
}

TEST_F(AnthropicRequestTest, Tools_KeepTheOrderOfTheToolsMap)
{
    ToolsMap tools;
    for (const QString& name : { "zeta", "alpha", "mid" })
    {
        tools.insert(name, toolData(name, "Does " + name + "."));
    }

    const QJsonObject body = build(weatherConversation(), tools);

    QStringList names;
    for (const QJsonValue& tool : body.value("tools").toArray())
    {
        names.append(tool.toObject().value("name").toString());
    }
    EXPECT_EQ(names, tools.keys());
}

TEST_F(AnthropicRequestTest, Request_IsByteIdenticalAcrossCalls)
{
    const ToolsMap tools = toolsMapOf({ "zeta", "alpha", "mid", "get_weather" });

    const QByteArray first = compact(build(weatherConversation(), tools, "high"));
    const QByteArray second = compact(build(weatherConversation(), tools, "high"));

    EXPECT_EQ(first, second);
}

TEST_F(AnthropicRequestTest, Request_IsByteIdenticalWhenToolsAreInsertedInAnotherOrder)
{
    ToolsMap forward;
    for (const QString& name : { "alpha", "mid", "zeta" })
    {
        forward.insert(name, toolData(name, "Does " + name + "."));
    }
    ToolsMap backward;
    for (const QString& name : { "zeta", "mid", "alpha" })
    {
        backward.insert(name, toolData(name, "Does " + name + "."));
    }

    EXPECT_EQ(compact(build(weatherConversation(), forward)),
              compact(build(weatherConversation(), backward)));
}

TEST_F(AnthropicRequestTest, UserText_BecomesATextBlock)
{
    const QJsonObject body
        = build({ Message { .role = "user", .content = "What is the weather in Paris?" } });

    EXPECT_TRUE(sameJson(sentMessage(body, 0), parseObject(R"json({
        "role": "user",
        "content": [{"type": "text", "text": "What is the weather in Paris?"}]
      })json")));
}

TEST_F(AnthropicRequestTest, AssistantText_BecomesATextBlock)
{
    const QJsonObject body = build({ Message { .role = "user", .content = "Hi" },
                                     Message { .role = "assistant", .content = "Hello!" },
                                     Message { .role = "user", .content = "Bye" } });

    EXPECT_TRUE(sameJson(sentMessage(body, 1), parseObject(R"json({
        "role": "assistant",
        "content": [{"type": "text", "text": "Hello!"}]
      })json")));
}

TEST_F(AnthropicRequestTest, AssistantToolCall_BecomesTextBlockThenToolUseWithObjectInput)
{
    const QJsonObject body = build(weatherConversation());

    EXPECT_TRUE(sameJson(sentMessage(body, 1), parseObject(R"json({
        "role": "assistant",
        "content": [
          {"type": "text", "text": "Let me check the weather."},
          {"type": "tool_use", "id": "toolu_abc123", "name": "get_weather",
           "input": {"location": "Paris"}}
        ]
      })json")));
}

TEST_F(AnthropicRequestTest, AssistantToolCall_WithoutTextHasNoTextBlock)
{
    const QJsonObject body
        = build({ Message { .role = "user", .content = "Hi" },
                  assistantCalling("",
                                   { agent_test::toolCall("toolu_1", "get_weather",
                                                          { { "location", "Rome" } }) }) });

    const QJsonArray content = sentMessage(body, 1).value("content").toArray();
    EXPECT_EQ(blockTypes(content), QStringList { "tool_use" });
}

TEST_F(AnthropicRequestTest, AssistantToolCalls_KeepTheirOrder)
{
    const QJsonObject body
        = build({ Message { .role = "user", .content = "Hi" },
                  assistantCalling("",
                                   { agent_test::toolCall("toolu_rome", "get_weather",
                                                          { { "location", "Rome" } }),
                                     agent_test::toolCall("toolu_paris", "get_weather",
                                                          { { "location", "Paris" } }) }) });

    const QJsonArray content = sentMessage(body, 1).value("content").toArray();
    ASSERT_EQ(content.size(), 2);
    EXPECT_EQ(content.at(0).toObject().value("id").toString(), "toolu_rome");
    EXPECT_EQ(content.at(1).toObject().value("id").toString(), "toolu_paris");
}

TEST_F(AnthropicRequestTest, ToolUseInput_KeepsNestedValuesAndTypes)
{
    const QVariantMap arguments { { "filters",
                                    QVariantMap { { "tags", QVariantList { "a", "b" } },
                                                  { "limit", 3 },
                                                  { "exact", true } } },
                                  { "title", "Trip" } };
    const QJsonObject body
        = build({ Message { .role = "user", .content = "Hi" },
                  assistantCalling("", { agent_test::toolCall("toolu_1", "search", arguments) }) });

    const QJsonObject block = sentMessage(body, 1).value("content").toArray().at(0).toObject();
    EXPECT_TRUE(sameJson(block.value("input").toObject(), parseObject(R"json({
        "filters": {"tags": ["a", "b"], "limit": 3, "exact": true},
        "title": "Trip"
      })json")));
}

TEST_F(AnthropicRequestTest, ToolUseInput_IsAnEmptyObjectWhenThereAreNoArguments)
{
    const QJsonObject body
        = build({ Message { .role = "user", .content = "Hi" },
                  assistantCalling("", { agent_test::toolCall("toolu_1", "list_apps", {}) }) });

    const QJsonValue input
        = sentMessage(body, 1).value("content").toArray().at(0).toObject().value("input");
    ASSERT_TRUE(input.isObject());
    EXPECT_TRUE(input.toObject().isEmpty());
}

TEST_F(AnthropicRequestTest, ToolResult_BecomesAUserMessageWithAToolResultBlock)
{
    const QJsonObject body = build(weatherConversation());

    EXPECT_TRUE(sameJson(sentMessage(body, 2), parseObject(R"json({
        "role": "user",
        "content": [{"type": "tool_result", "tool_use_id": "toolu_abc123",
                     "content": "72°F and sunny"}]
      })json")));
}

TEST_F(AnthropicRequestTest, ToolResult_ContentIsPassedThroughAsText)
{
    const QString content = R"({"value":{"temperature":72,"unit":"F"}})";
    const QJsonObject body
        = build({ Message { .role = "user", .content = "Hi" },
                  assistantCalling("", { agent_test::toolCall("toolu_1", "get_weather", {}) }),
                  Message { .role = "tool", .content = content, .tool_call_id = "toolu_1" } });

    const QJsonObject block = sentMessage(body, 2).value("content").toArray().at(0).toObject();
    EXPECT_EQ(block.value("content").toString(), content);
}

TEST_F(AnthropicRequestTest, ConsecutiveToolResults_ShareOneUserMessage)
{
    const QJsonObject body = build(
        { Message { .role = "user", .content = "Weather in Paris and Rome?" },
          assistantCalling(
              "",
              { agent_test::toolCall("toolu_paris", "get_weather", { { "location", "Paris" } }),
                agent_test::toolCall("toolu_rome", "get_weather", { { "location", "Rome" } }) }),
          Message { .role = "tool",
                    .content = QString::fromUtf8("72°F and sunny"),
                    .tool_call_id = "toolu_paris" },
          Message { .role = "tool",
                    .content = QString::fromUtf8("65°F and cloudy"),
                    .tool_call_id = "toolu_rome" } });

    ASSERT_EQ(sentMessages(body).size(), 3);
    EXPECT_TRUE(sameJson(sentMessage(body, 2), parseObject(R"json({
        "role": "user",
        "content": [
          {"type": "tool_result", "tool_use_id": "toolu_paris", "content": "72°F and sunny"},
          {"type": "tool_result", "tool_use_id": "toolu_rome", "content": "65°F and cloudy"}
        ]
      })json")));
}

TEST_F(AnthropicRequestTest, ThreeConsecutiveToolResults_ShareOneUserMessage)
{
    const QVector<ToolCall> calls { agent_test::toolCall("toolu_1", "a", {}),
                                    agent_test::toolCall("toolu_2", "b", {}),
                                    agent_test::toolCall("toolu_3", "c", {}) };
    const QJsonObject body
        = build({ Message { .role = "user", .content = "Go" }, assistantCalling("", calls),
                  Message { .role = "tool", .content = "1", .tool_call_id = "toolu_1" },
                  Message { .role = "tool", .content = "2", .tool_call_id = "toolu_2" },
                  Message { .role = "tool", .content = "3", .tool_call_id = "toolu_3" } });

    ASSERT_EQ(sentMessages(body).size(), 3);
    const QJsonArray results = sentMessage(body, 2).value("content").toArray();
    EXPECT_EQ(blockTypes(results), (QStringList { "tool_result", "tool_result", "tool_result" }));
    EXPECT_EQ(results.at(2).toObject().value("tool_use_id").toString(), "toolu_3");
}

TEST_F(AnthropicRequestTest, ToolResultsSeparatedByAnAssistantMessage_StayInSeparateUserMessages)
{
    const QJsonObject body
        = build({ Message { .role = "user", .content = "Go" },
                  assistantCalling("", { agent_test::toolCall("toolu_1", "a", {}) }),
                  Message { .role = "tool", .content = "1", .tool_call_id = "toolu_1" },
                  assistantCalling("", { agent_test::toolCall("toolu_2", "b", {}) }),
                  Message { .role = "tool", .content = "2", .tool_call_id = "toolu_2" } });

    ASSERT_EQ(sentMessages(body).size(), 5);
    EXPECT_EQ(sentMessage(body, 2).value("content").toArray().size(), 1);
    EXPECT_EQ(sentMessage(body, 4).value("content").toArray().size(), 1);
    EXPECT_EQ(sentMessage(body, 4)
                  .value("content")
                  .toArray()
                  .at(0)
                  .toObject()
                  .value("tool_use_id")
                  .toString(),
              "toolu_2");
}

TEST_F(AnthropicRequestTest, ToolConversation_AlternatesUserAndAssistantRoles)
{
    const QJsonObject body
        = build({ Message { .role = "system", .content = "Be brief." },
                  Message { .role = "user", .content = "Go" },
                  assistantCalling("",
                                   { agent_test::toolCall("toolu_1", "a", {}),
                                     agent_test::toolCall("toolu_2", "b", {}) }),
                  Message { .role = "tool", .content = "1", .tool_call_id = "toolu_1" },
                  Message { .role = "tool", .content = "2", .tool_call_id = "toolu_2" },
                  Message { .role = "assistant", .content = "Done." },
                  Message { .role = "user", .content = "Thanks" } });

    QStringList roles;
    for (const QJsonValue& sent : sentMessages(body))
    {
        roles.append(sent.toObject().value("role").toString());
    }
    EXPECT_EQ(roles, (QStringList { "user", "assistant", "user", "assistant", "user" }));
}

TEST_F(AnthropicRequestTest, Conversation_MatchesTheDocumentedToolUseExample)
{
    const QJsonObject body = build(weatherConversation(), weatherTools());

    EXPECT_TRUE(sameJson(sentMessages(body), parseArray(R"json([
        {"role": "user", "content": [{"type": "text", "text": "What is the weather in Paris?"}]},
        {"role": "assistant", "content": [
          {"type": "text", "text": "Let me check the weather."},
          {"type": "tool_use", "id": "toolu_abc123", "name": "get_weather", "input": {"location": "Paris"}}
        ]},
        {"role": "user", "content": [
          {"type": "tool_result", "tool_use_id": "toolu_abc123", "content": "72°F and sunny"}
        ]}
      ])json")));
}

TEST_F(AnthropicRequestTest, RawContent_IsReplayedVerbatimAsAssistantContent)
{
    Message assistant { .role = "assistant", .content = "Let me check the weather." };
    assistant.raw_content = parseArray(kRawAssistantContent);

    const QJsonObject body
        = build({ Message { .role = "user", .content = "What is the weather in Paris?" }, assistant,
                  Message { .role = "tool", .content = "sunny", .tool_call_id = "toolu_abc123" } });

    EXPECT_TRUE(sameJson(sentMessage(body, 1).value("content").toArray(),
                         parseArray(kRawAssistantContent)));
    EXPECT_EQ(sentMessage(body, 1).value("role").toString(), "assistant");
}

TEST_F(AnthropicRequestTest, RawContent_KeepsThinkingBlocksUnchangedAndInOrder)
{
    Message assistant { .role = "assistant", .content = "Let me check the weather." };
    assistant.raw_content = parseArray(kRawAssistantContent);

    const QJsonObject body
        = build({ Message { .role = "user", .content = "Hi" }, assistant,
                  Message { .role = "tool", .content = "sunny", .tool_call_id = "toolu_abc123" } });

    const QJsonArray content = sentMessage(body, 1).value("content").toArray();
    EXPECT_EQ(blockTypes(content),
              (QStringList { "thinking", "redacted_thinking", "text", "tool_use" }));
    EXPECT_EQ(content.at(0).toObject().value("thinking").toString(),
              "The user wants the weather, so I should call the tool.");
    EXPECT_EQ(content.at(0).toObject().value("signature").toString(),
              "EuYBCkQYAiJAbm9uLWNyeXB0b2dyYXBoaWMtc2lnbmF0dXJl");
    EXPECT_EQ(content.at(1).toObject().value("data").toString(),
              "EmwKAhgBEgy3va3pzix/LafPsn4aDFIT2Xlxh0L5L8rLVyIw");
}

TEST_F(AnthropicRequestTest, RawContent_WinsOverContentAndToolCalls)
{
    Message assistant = assistantCalling(
        "edited text", { agent_test::toolCall("toolu_other", "other_tool", { { "x", 1 } }) });
    assistant.raw_content = parseArray(kRawAssistantContent);

    const QJsonObject body
        = build({ Message { .role = "user", .content = "Hi" }, assistant,
                  Message { .role = "tool", .content = "sunny", .tool_call_id = "toolu_abc123" } });

    EXPECT_TRUE(sameJson(sentMessage(body, 1).value("content").toArray(),
                         parseArray(kRawAssistantContent)));
}

TEST_F(AnthropicRequestTest, RawContent_OfOneAssistantMessageDoesNotAffectTheOthers)
{
    Message withThinking { .role = "assistant", .content = "Let me check the weather." };
    withThinking.raw_content = parseArray(kRawAssistantContent);

    const QJsonObject body
        = build({ Message { .role = "user", .content = "Hi" }, withThinking,
                  Message { .role = "tool", .content = "sunny", .tool_call_id = "toolu_abc123" },
                  Message { .role = "assistant", .content = "It is sunny." },
                  Message { .role = "user", .content = "Thanks" } });

    EXPECT_TRUE(sameJson(sentMessage(body, 3), parseObject(R"json({
        "role": "assistant",
        "content": [{"type": "text", "text": "It is sunny."}]
      })json")));
}

TEST_F(AnthropicRequestTest, Effort_High_BecomesOutputConfigEffort)
{
    const QJsonObject body = build({ Message { .role = "user", .content = "Hi" } }, {}, "high");

    EXPECT_TRUE(sameJson(body.value("output_config").toObject(),
                         parseObject(R"json({"effort": "high"})json")));
}

TEST_F(AnthropicRequestTest, Effort_Empty_OmitsOutputConfig)
{
    const QJsonObject body = build({ Message { .role = "user", .content = "Hi" } }, {}, "");

    EXPECT_FALSE(body.contains("output_config"));
}

TEST_F(AnthropicRequestTest, Effort_Minimal_MapsToLow)
{
    for (const QString& configured : { "minimal", "Minimal", "MINIMAL" })
    {
        SCOPED_TRACE(configured.toStdString());
        EXPECT_EQ(effortFor(configured), "low");
    }
}

TEST_F(AnthropicRequestTest, Effort_LowMediumAndHigh_AreKept)
{
    EXPECT_EQ(effortFor("low"), "low");
    EXPECT_EQ(effortFor("medium"), "medium");
    EXPECT_EQ(effortFor("high"), "high");
}

TEST_F(AnthropicRequestTest, Effort_IsLowercased)
{
    EXPECT_EQ(effortFor("Low"), "low");
    EXPECT_EQ(effortFor("Medium"), "medium");
    EXPECT_EQ(effortFor("HIGH"), "high");
}

TEST_F(AnthropicRequestTest, Effort_OtherValuesPassThroughLowercased)
{
    EXPECT_EQ(effortFor("xhigh"), "xhigh");
    EXPECT_EQ(effortFor("XHigh"), "xhigh");
    EXPECT_EQ(effortFor("max"), "max");
    EXPECT_EQ(effortFor("Max"), "max");
}

TEST_F(AnthropicRequestTest, NoThinkingField_EvenWithEffort)
{
    const QJsonObject body = build(weatherConversation(), weatherTools(), "high");

    EXPECT_FALSE(body.contains("thinking"));
}

TEST_F(AnthropicRequestTest, NoSamplingFields)
{
    const QJsonObject body = build(weatherConversation(), weatherTools(), "high");

    EXPECT_FALSE(body.contains("temperature"));
    EXPECT_FALSE(body.contains("top_p"));
    EXPECT_FALSE(body.contains("top_k"));
}

TEST_F(AnthropicRequestTest, NoToolChoice)
{
    const QJsonObject body = build(weatherConversation(), weatherTools(), "high");

    EXPECT_FALSE(body.contains("tool_choice"));
}

class AnthropicResponseTest : public ::testing::Test
{
protected:
    static Completion complete(const char* response)
    {
        return anthropic::completionFrom(parseObject(response));
    }

    static Completion withStopReason(const QString& stopReason)
    {
        QJsonObject response = parseObject(kTextResponse);
        response["stop_reason"] = stopReason;
        return anthropic::completionFrom(response);
    }

    static Completion withError(const QString& type)
    {
        const QJsonObject error { { "type", type }, { "message", "Something went wrong" } };
        return anthropic::completionFrom(
            QJsonObject { { "type", "error" },
                          { "error", error },
                          { "request_id", "req_011CSHoEeqs5C35K2UUqR7Fy" } });
    }

    static void expectFailed(const Completion& completion, const QString& token)
    {
        EXPECT_EQ(completion.error, token);
        EXPECT_TRUE(completion.choices.isEmpty());
    }
};

TEST_F(AnthropicResponseTest, Text_BecomesTheAssistantMessageContent)
{
    const Completion completion = complete(kTextResponse);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).index, 0);
    EXPECT_EQ(completion.choices.at(0).message.role, "assistant");
    EXPECT_EQ(completion.choices.at(0).message.content, "The capital of France is Paris.");
    EXPECT_TRUE(completion.choices.at(0).message.tool_calls.isEmpty());
    EXPECT_TRUE(completion.error.isEmpty());
}

TEST_F(AnthropicResponseTest, SeveralTextBlocks_AreConcatenatedWithoutSeparator)
{
    QJsonObject response = parseObject(kTextResponse);
    response["content"] = parseArray(R"json([
        {"type": "text", "text": "The capital of "},
        {"type": "text", "text": "France is Paris."}
      ])json");

    const Completion completion = anthropic::completionFrom(response);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).message.content, "The capital of France is Paris.");
}

TEST_F(AnthropicResponseTest, TextAroundToolUse_IsConcatenatedInOrder)
{
    QJsonObject response = parseObject(kToolUseResponse);
    response["content"] = parseArray(R"json([
        {"type": "text", "text": "Checking "},
        {"type": "tool_use", "id": "toolu_abc123", "name": "get_weather", "input": {"location": "Paris"}},
        {"type": "text", "text": "now."}
      ])json");

    const Completion completion = anthropic::completionFrom(response);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).message.content, "Checking now.");
}

TEST_F(AnthropicResponseTest, EmptyContent_IsAnEmptyStopMessage)
{
    QJsonObject response = parseObject(kTextResponse);
    response["content"] = QJsonArray();

    const Completion completion = anthropic::completionFrom(response);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_TRUE(completion.choices.at(0).message.content.isEmpty());
    EXPECT_EQ(completion.choices.at(0).finish_reason, "stop");
    EXPECT_TRUE(completion.error.isEmpty());
}

TEST_F(AnthropicResponseTest, ToolUse_BecomesAToolCallWithIdNameAndArguments)
{
    const Completion completion = complete(kToolUseResponse);

    ASSERT_EQ(completion.choices.size(), 1);
    const QVector<ToolCall>& calls = completion.choices.at(0).message.tool_calls;
    ASSERT_EQ(calls.size(), 1);
    EXPECT_EQ(calls.at(0).id, "toolu_abc123");
    EXPECT_EQ(calls.at(0).name, "get_weather");
    EXPECT_EQ(calls.at(0).arguments.value("location").toString(), "Paris");
    EXPECT_EQ(calls.at(0).arguments.size(), 1);
}

TEST_F(AnthropicResponseTest, ToolUse_KeepsTheTextBeforeTheCall)
{
    const Completion completion = complete(kToolUseResponse);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).message.content, "Let me check the weather.");
}

TEST_F(AnthropicResponseTest, ToolUse_ArgumentsSerializeAsCompactJsonOfTheInput)
{
    QJsonObject response = parseObject(kToolUseResponse);
    response["content"] = parseArray(R"json([
        {"type": "tool_use", "id": "toolu_1", "name": "search",
         "input": {"location": "Paris", "days": [1, 2], "units": {"temp": "C"}}}
      ])json");

    const Completion completion = anthropic::completionFrom(response);

    ASSERT_EQ(completion.choices.size(), 1);
    ASSERT_EQ(completion.choices.at(0).message.tool_calls.size(), 1);
    const QJsonObject serialized = completion.choices.at(0).message.tool_calls.at(0).toJsonObject();
    EXPECT_EQ(serialized.value("function").toObject().value("arguments").toString(),
              R"({"days":[1,2],"location":"Paris","units":{"temp":"C"}})");
}

TEST_F(AnthropicResponseTest, ToolUse_NestedInputSurvivesInTheArguments)
{
    QJsonObject response = parseObject(kToolUseResponse);
    const QJsonObject input = parseObject(R"json({
        "filters": {"tags": ["a", "b"], "limit": 3, "exact": true},
        "title": "Trip"
      })json");
    response["content"] = QJsonArray { QJsonObject {
        { "type", "tool_use" }, { "id", "toolu_1" }, { "name", "search" }, { "input", input } } };

    const Completion completion = anthropic::completionFrom(response);

    ASSERT_EQ(completion.choices.size(), 1);
    ASSERT_EQ(completion.choices.at(0).message.tool_calls.size(), 1);
    EXPECT_TRUE(sameJson(
        QJsonObject::fromVariantMap(completion.choices.at(0).message.tool_calls.at(0).arguments),
        input));
}

TEST_F(AnthropicResponseTest, ToolUse_EmptyInputGivesEmptyArguments)
{
    QJsonObject response = parseObject(kToolUseResponse);
    response["content"] = parseArray(R"json([
        {"type": "tool_use", "id": "toolu_1", "name": "list_apps", "input": {}}
      ])json");

    const Completion completion = anthropic::completionFrom(response);

    ASSERT_EQ(completion.choices.size(), 1);
    ASSERT_EQ(completion.choices.at(0).message.tool_calls.size(), 1);
    const ToolCall& call = completion.choices.at(0).message.tool_calls.at(0);
    EXPECT_TRUE(call.arguments.isEmpty());
    EXPECT_EQ(call.toJsonObject().value("function").toObject().value("arguments").toString(), "{}");
}

TEST_F(AnthropicResponseTest, SeveralToolUses_KeepTheirOrder)
{
    QJsonObject response = parseObject(kToolUseResponse);
    response["content"] = parseArray(R"json([
        {"type": "tool_use", "id": "toolu_rome", "name": "get_weather", "input": {"location": "Rome"}},
        {"type": "tool_use", "id": "toolu_paris", "name": "get_weather", "input": {"location": "Paris"}}
      ])json");

    const Completion completion = anthropic::completionFrom(response);

    ASSERT_EQ(completion.choices.size(), 1);
    const QVector<ToolCall>& calls = completion.choices.at(0).message.tool_calls;
    ASSERT_EQ(calls.size(), 2);
    EXPECT_EQ(calls.at(0).id, "toolu_rome");
    EXPECT_EQ(calls.at(1).id, "toolu_paris");
}

TEST_F(AnthropicResponseTest, ToolUseOnly_HasEmptyContent)
{
    QJsonObject response = parseObject(kToolUseResponse);
    response["content"] = parseArray(R"json([
        {"type": "tool_use", "id": "toolu_1", "name": "list_apps", "input": {}}
      ])json");

    const Completion completion = anthropic::completionFrom(response);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_TRUE(completion.choices.at(0).message.content.isEmpty());
}

TEST_F(AnthropicResponseTest, ThinkingBlocks_AreNotPartOfTheText)
{
    const Completion completion = complete(kThinkingToolUseResponse);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).message.content, "Let me check the weather.");
    EXPECT_FALSE(completion.choices.at(0).message.content.contains("The user wants"));
}

TEST_F(AnthropicResponseTest, ThinkingBlocks_AreNotToolCalls)
{
    const Completion completion = complete(kThinkingToolUseResponse);

    ASSERT_EQ(completion.choices.size(), 1);
    ASSERT_EQ(completion.choices.at(0).message.tool_calls.size(), 1);
    EXPECT_EQ(completion.choices.at(0).message.tool_calls.at(0).id, "toolu_abc123");
}

TEST_F(AnthropicResponseTest, RawContent_HoldsEveryBlockVerbatimIncludingThinking)
{
    const Completion completion = complete(kThinkingToolUseResponse);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_TRUE(
        sameJson(completion.choices.at(0).message.raw_content, parseArray(kRawAssistantContent)));
}

TEST_F(AnthropicResponseTest, RawContent_OfATextOnlyResponseHoldsTheTextBlock)
{
    const Completion completion = complete(kTextResponse);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_TRUE(sameJson(
        completion.choices.at(0).message.raw_content,
        parseArray(R"json([{"type": "text", "text": "The capital of France is Paris."}])json")));
}

TEST_F(AnthropicResponseTest, RawContent_KeepsFieldsTheClientDoesNotKnow)
{
    QJsonObject response = parseObject(kTextResponse);
    const QJsonArray content = parseArray(R"json([
        {"type": "text", "text": "Hi", "citations": null},
        {"type": "future_block", "payload": {"a": [1, 2, 3]}}
      ])json");
    response["content"] = content;

    const Completion completion = anthropic::completionFrom(response);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_TRUE(sameJson(completion.choices.at(0).message.raw_content, content));
}

TEST_F(AnthropicResponseTest, StopReasonToolUse_IsFinishReasonToolCalls)
{
    const Completion completion = complete(kToolUseResponse);

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).finish_reason, "tool_calls");
}

TEST_F(AnthropicResponseTest, StopReasonEndTurn_IsFinishReasonStop)
{
    const Completion completion = withStopReason("end_turn");

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).finish_reason, "stop");
}

TEST_F(AnthropicResponseTest, StopReasonStopSequence_IsFinishReasonStop)
{
    const Completion completion = withStopReason("stop_sequence");

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).finish_reason, "stop");
}

TEST_F(AnthropicResponseTest, StopReasonMaxTokens_IsFinishReasonLength)
{
    const Completion completion = withStopReason("max_tokens");

    ASSERT_EQ(completion.choices.size(), 1);
    EXPECT_EQ(completion.choices.at(0).finish_reason, "length");
    EXPECT_TRUE(completion.error.isEmpty());
}

TEST_F(AnthropicResponseTest, StopReasonRefusal_IsAFailedCompletion)
{
    QJsonObject response = parseObject(kTextResponse);
    response["stop_reason"] = "refusal";
    response["content"] = QJsonArray();
    response["stop_details"] = parseObject(R"json({
        "type": "refusal",
        "category": "cyber",
        "explanation": "This request was declined by a safety classifier."
      })json");

    expectFailed(anthropic::completionFrom(response), "refusal");
}

TEST_F(AnthropicResponseTest, StopReasonRefusal_WithoutStopDetailsIsStillFailed)
{
    QJsonObject response = parseObject(kTextResponse);
    response["stop_reason"] = "refusal";
    response["content"] = QJsonArray();

    expectFailed(anthropic::completionFrom(response), "refusal");
}

TEST_F(AnthropicResponseTest, StopReasonRefusal_AfterPartialOutputDiscardsTheOutput)
{
    QJsonObject response = parseObject(kTextResponse);
    response["stop_reason"] = "refusal";
    response["content"] = parseArray(R"json([{"type": "text", "text": "Here is how to"}])json");
    response["stop_details"]
        = parseObject(R"json({"type": "refusal", "category": null, "explanation": null})json");

    expectFailed(anthropic::completionFrom(response), "refusal");
}

TEST_F(AnthropicResponseTest, Usage_PromptTokensSumInputCacheWriteAndCacheRead)
{
    const Completion completion = complete(kCachedUsageResponse);

    EXPECT_EQ(completion.usage.prompt_tokens, 21 + 1000 + 5000);
}

TEST_F(AnthropicResponseTest, Usage_CompletionTokensAreTheOutputTokens)
{
    const Completion completion = complete(kCachedUsageResponse);

    EXPECT_EQ(completion.usage.completion_tokens, 50);
}

TEST_F(AnthropicResponseTest, Usage_TotalIsPromptPlusCompletion)
{
    const Completion completion = complete(kCachedUsageResponse);

    EXPECT_EQ(completion.usage.total_tokens, 21 + 1000 + 5000 + 50);
}

TEST_F(AnthropicResponseTest, Usage_ContextTokensEqualPromptTokens)
{
    const Completion completion = complete(kCachedUsageResponse);

    EXPECT_EQ(completion.usage.context_tokens, completion.usage.prompt_tokens);
    EXPECT_EQ(completion.usage.context_tokens, 6021);
}

TEST_F(AnthropicResponseTest, Usage_CacheCreationBreakdownIsNotCountedTwice)
{
    const Completion completion = complete(kCachedUsageResponse);

    EXPECT_EQ(completion.usage.prompt_tokens, 6021);
}

TEST_F(AnthropicResponseTest, Usage_WithoutCacheTokensIsInputPlusOutput)
{
    const Completion completion = complete(kTextResponse);

    EXPECT_EQ(completion.usage.prompt_tokens, 14);
    EXPECT_EQ(completion.usage.completion_tokens, 9);
    EXPECT_EQ(completion.usage.total_tokens, 23);
    EXPECT_EQ(completion.usage.context_tokens, 14);
}

TEST_F(AnthropicResponseTest, Usage_AbsentCacheCountersCountAsZero)
{
    QJsonObject response = parseObject(kTextResponse);
    response["usage"] = parseObject(R"json({"input_tokens": 14, "output_tokens": 9})json");

    const Completion completion = anthropic::completionFrom(response);

    EXPECT_EQ(completion.usage.prompt_tokens, 14);
    EXPECT_EQ(completion.usage.total_tokens, 23);
}

TEST_F(AnthropicResponseTest, Usage_NullCacheCountersCountAsZero)
{
    QJsonObject response = parseObject(kTextResponse);
    response["usage"] = parseObject(R"json({
        "input_tokens": 14, "output_tokens": 9,
        "cache_creation_input_tokens": null, "cache_read_input_tokens": null
      })json");

    const Completion completion = anthropic::completionFrom(response);

    EXPECT_EQ(completion.usage.prompt_tokens, 14);
    EXPECT_EQ(completion.usage.total_tokens, 23);
}

TEST_F(AnthropicResponseTest, Usage_OfAToolUseResponseIsCounted)
{
    const Completion completion = complete(kToolUseResponse);

    EXPECT_EQ(completion.usage.prompt_tokens, 21);
    EXPECT_EQ(completion.usage.completion_tokens, 38);
    EXPECT_EQ(completion.usage.total_tokens, 59);
}

TEST_F(AnthropicResponseTest, ErrorBody_MatchesTheDocumentedShapeAndFails)
{
    const Completion completion = complete(R"json({
        "type": "error",
        "error": {"type": "invalid_request_error",
                  "message": "messages: roles must alternate between \"user\" and \"assistant\""},
        "request_id": "req_011CSHoEeqs5C35K2UUqR7Fy"
      })json");

    expectFailed(completion, "unknown_error");
}

TEST_F(AnthropicResponseTest, ErrorBody_AuthenticationAndPermission_AreInvalidKey)
{
    expectFailed(withError("authentication_error"), "invalid_key");
    expectFailed(withError("permission_error"), "invalid_key");
}

TEST_F(AnthropicResponseTest, ErrorBody_Billing_IsInsufficientCredits)
{
    expectFailed(withError("billing_error"), "insufficient_credits");
}

TEST_F(AnthropicResponseTest, ErrorBody_RateLimit_IsRateLimit)
{
    expectFailed(withError("rate_limit_error"), "rate_limit");
}

TEST_F(AnthropicResponseTest, ErrorBody_ServiceSideTypes_AreServerError)
{
    expectFailed(withError("api_error"), "server_error");
    expectFailed(withError("overloaded_error"), "server_error");
    expectFailed(withError("timeout_error"), "server_error");
}

TEST_F(AnthropicResponseTest, ErrorBody_RequestSideTypes_AreUnknownError)
{
    expectFailed(withError("invalid_request_error"), "unknown_error");
    expectFailed(withError("not_found_error"), "unknown_error");
    expectFailed(withError("request_too_large"), "unknown_error");
}

TEST_F(AnthropicResponseTest, ErrorBody_UnknownType_IsUnknownError)
{
    expectFailed(withError("something_new_error"), "unknown_error");
}

TEST_F(AnthropicResponseTest, ErrorBody_WithoutErrorObject_IsUnknownError)
{
    expectFailed(anthropic::completionFrom(parseObject(R"json({"type": "error"})json")),
                 "unknown_error");
}

TEST_F(AnthropicResponseTest, ErrorBody_DoesNotLeakTheServerMessageIntoTheToken)
{
    const Completion completion = withError("authentication_error");

    EXPECT_FALSE(completion.error.contains("Something went wrong"));
}

TEST(AnthropicRoundTripTest, ResponseContent_IsReplayedVerbatimInTheNextRequest)
{
    const Completion completion = anthropic::completionFrom(parseObject(kThinkingToolUseResponse));
    ASSERT_EQ(completion.choices.size(), 1);

    const Messages messages { Message { .role = "user",
                                        .content = "What is the weather in Paris?" },
                              completion.choices.at(0).message,
                              Message { .role = "tool",
                                        .content = QString::fromUtf8("72°F and sunny"),
                                        .tool_call_id = "toolu_abc123" } };
    const QJsonObject body
        = anthropic::requestBody(ModelConfig { .model = "claude-opus-5-5" }, messages, {});

    const QJsonArray sent = body.value("messages").toArray();
    ASSERT_EQ(sent.size(), 3);
    EXPECT_TRUE(sameJson(sent.at(1).toObject().value("content").toArray(),
                         parseArray(kRawAssistantContent)));
}

TEST(AnthropicRoundTripTest, ToolResultsOfAReplayedAssistantMessage_ShareOneUserMessage)
{
    QJsonObject response = parseObject(kThinkingToolUseResponse);
    response["content"] = parseArray(R"json([
        {"type": "thinking", "thinking": "Two cities.", "signature": "c2lnbmF0dXJl"},
        {"type": "tool_use", "id": "toolu_rome", "name": "get_weather", "input": {"location": "Rome"}},
        {"type": "tool_use", "id": "toolu_paris", "name": "get_weather", "input": {"location": "Paris"}}
      ])json");
    const Completion completion = anthropic::completionFrom(response);
    ASSERT_EQ(completion.choices.size(), 1);

    const Messages messages {
        Message { .role = "user", .content = "Rome and Paris?" }, completion.choices.at(0).message,
        Message { .role = "tool", .content = "sunny", .tool_call_id = "toolu_rome" },
        Message { .role = "tool", .content = "rainy", .tool_call_id = "toolu_paris" }
    };
    const QJsonObject body
        = anthropic::requestBody(ModelConfig { .model = "claude-opus-5-5" }, messages, {});

    const QJsonArray sent = body.value("messages").toArray();
    ASSERT_EQ(sent.size(), 3);
    EXPECT_EQ(sent.at(2).toObject().value("content").toArray().size(), 2);
}
