#include "structures.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace
{

const char* kJsonIdKey = "id";
const char* kJsonNameKey = "name";
const char* kJsonArgumentsKey = "arguments";
const char* kJsonTypeKey = "type";

const char* kJsonRoleKey = "role";
const char* kJsonContentKey = "content";
const char* kJsonToolCallIdKey = "tool_call_id";

const char* kJsonPromptTokensKey = "prompt_tokens";
const char* kJsonCompletionTokensKey = "completion_tokens";
const char* kJsonTotalTokensKey = "total_tokens";

const char* kJsonMessageKey = "message";
const char* kJsonFinishReasonKey = "finish_reason";
const char* kJsonIndexKey = "index";

const char* kJsonObjectKey = "object";
const char* kJsonCreatedKey = "created";
const char* kJsonModelKey = "model";
const char* kJsonUsageKey = "usage";
const char* kJsonChoicesKey = "choices";

const char* kJsonToolCallsKey = "tool_calls";
const char* kJsonFunctionKey = "function";

QVariantMap parseArguments(const QJsonValue& json)
{
    auto doc = QJsonDocument::fromJson(json.toString().toUtf8());
    return doc.object().toVariantMap();
}

QString argumentsToString(const QVariantMap& map)
{
    QJsonObject jsonObject = QJsonObject::fromVariantMap(map);
    QJsonDocument doc(jsonObject);

    return doc.toJson(QJsonDocument::Compact);
}

}

QJsonObject toJsonObject(const QVariantMap& map)
{
    QJsonObject json;
    for (const auto& key : map.keys())
    {
        json[key] = QJsonValue::fromVariant(map.value(key));
    }
    return json;
}

QVariantMap fromJsonObject(const QJsonObject& json)
{
    QVariantMap map;
    for (const auto& key : json.keys())
    {
        map[key] = json[key].toVariant();
    }
    return map;
}

QJsonObject ToolCall::toJsonObject() const
{
    QJsonObject json;
    json[kJsonIdKey] = id;
    json[kJsonTypeKey] = type;

    QJsonObject jsonFunction;
    jsonFunction[kJsonNameKey] = name;
    jsonFunction[kJsonArgumentsKey] = argumentsToString(arguments);

    json[kJsonFunctionKey] = jsonFunction;

    return json;
}

ToolCall ToolCall::fromJsonObject(const QJsonObject& json)
{
    ToolCall toolCall;
    toolCall.id = json[kJsonIdKey].toString();
    QJsonObject jsonFunction = json[kJsonFunctionKey].toObject();
    toolCall.type = json[kJsonTypeKey].toString();

    toolCall.name = jsonFunction[kJsonNameKey].toString();

    toolCall.arguments = parseArguments(jsonFunction[kJsonArgumentsKey]);
    return toolCall;
}

QJsonObject Message::toJsonObject() const
{
    QJsonObject json;
    json[kJsonRoleKey] = role;
    json[kJsonContentKey] = content;

    if (!tool_call_id.isEmpty())
    {
        json[kJsonToolCallIdKey] = tool_call_id;
    }

    if (tool_calls.size() > 0)
    {
        QJsonArray toolCallsArray;
        for (const auto& tool_call : tool_calls)
        {
            toolCallsArray.append(tool_call.toJsonObject());
        }
        json[kJsonToolCallsKey] = toolCallsArray;
    }

    return json;
}

Message Message::fromJsonObject(const QJsonObject& json)
{
    Message msg;
    msg.role = json[kJsonRoleKey].toString();
    msg.content = json[kJsonContentKey].toString();

    for (const auto& toolCallJson : json[kJsonToolCallsKey].toArray())
    {
        msg.tool_calls.append(
            ToolCall::fromJsonObject(toolCallJson.toObject()));
    }

    return msg;
}

QJsonArray toJsonArray(const Messages& messages)
{
    QJsonArray jsonArray;
    for (const auto& message : messages)
    {
        jsonArray.append(message.toJsonObject());
    }
    return jsonArray;
}

Messages fromJsonArray(const QJsonArray& json)
{
    Messages messages;
    for (const auto& jsonValue : json)
    {
        messages.append(Message::fromJsonObject(jsonValue.toObject()));
    }
    return messages;
}

QJsonObject Usage::toJsonObject() const
{
    QJsonObject json;
    json[kJsonPromptTokensKey] = prompt_tokens;
    json[kJsonCompletionTokensKey] = completion_tokens;
    json[kJsonTotalTokensKey] = total_tokens;
    return json;
}

Usage Usage::fromJsonObject(const QJsonObject& json)
{
    Usage usage;
    usage.prompt_tokens = json[kJsonPromptTokensKey].toInt();
    usage.completion_tokens = json[kJsonCompletionTokensKey].toInt();
    usage.total_tokens = json[kJsonTotalTokensKey].toInt();
    return usage;
}

QJsonObject Choice::toJsonObject() const
{
    QJsonObject json;
    json[kJsonMessageKey] = message.toJsonObject();
    json[kJsonFinishReasonKey] = finish_reason;
    json[kJsonIndexKey] = index;
    return json;
}

Choice Choice::fromJsonObject(const QJsonObject& json)
{
    Choice choice;
    choice.message = Message::fromJsonObject(json[kJsonMessageKey].toObject());
    choice.finish_reason = json[kJsonFinishReasonKey].toString();
    choice.index = json[kJsonIndexKey].toInt();
    return choice;
}

QJsonObject Completion::toJsonObject() const
{
    QJsonObject json;
    json[kJsonIdKey] = id;
    json[kJsonObjectKey] = object;
    json[kJsonCreatedKey] = static_cast<qint64>(created);
    json[kJsonModelKey] = model;
    json[kJsonUsageKey] = usage.toJsonObject();
    QJsonArray choiceArray;
    for (const auto& choice : choices)
    {
        choiceArray.append(choice.toJsonObject());
    }
    json[kJsonChoicesKey] = choiceArray;
    return json;
}

Completion Completion::fromJsonObject(const QJsonObject& json)
{
    Completion completion;
    completion.id = json[kJsonIdKey].toString();
    completion.object = json[kJsonObjectKey].toString();
    completion.created = json[kJsonCreatedKey].toVariant().toLongLong();
    completion.model = json[kJsonModelKey].toString();
    completion.usage = Usage::fromJsonObject(json[kJsonUsageKey].toObject());
    for (const auto& choiceJson : json[kJsonChoicesKey].toArray())
    {
        completion.choices.append(
            Choice::fromJsonObject(choiceJson.toObject()));
    }
    return completion;
}

QJsonArray toJsonArray(const ToolsMap& toolsMap)
{
    QJsonArray json;
    for (const auto& toolData : toolsMap)
    {
        json.append(toolData.json);
    }
    return json;
}
