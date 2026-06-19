#include "agent.h"

#include <functional>
#include <tuple>
#include <type_traits>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>

#include "logging.h"

namespace
{
static const char* kJsonTypeKey = "type";
static const char* kJsonFunctionKey = "function";
static const char* kJsonStrictKey = "strict";
static const char* kJsonNameKey = "name";
static const char* kJsonDescriptionKey = "description";
static const char* kJsonParametersKey = "parameters";
static const char* kJsonReturnKey = "return";

static const QRegularExpression nameRegex(R"(@brief\s+(\w+))");
static const QRegularExpression descriptionRegex(R"(@details\s+(.+))");
static const QRegularExpression paramRegex(R"(@param\s+(\w+)\s+\(([^)]+)\)\s+(.+))");
static const QRegularExpression returnRegex(R"(@return\s+(.+))");

const int kMaxToolCallsCount = 20;

QJsonObject parseDocstring(const QString& docstring)
{
    QJsonObject result;
    result[kJsonTypeKey] = "function";

    QJsonObject functionObject;
    functionObject[kJsonStrictKey] = true;

    QRegularExpressionMatch nameMatch = nameRegex.match(docstring);
    if (!nameMatch.hasMatch())
    {
        qCWarning(AgentLogic) << "Name doesn't match";
        return {};
    }
    functionObject[kJsonNameKey] = nameMatch.captured(1).trimmed();

    QRegularExpressionMatch descriptionMatch = descriptionRegex.match(docstring);
    if (!descriptionMatch.hasMatch())
    {
        qCWarning(AgentLogic) << "Description doesn't match";
        return {};
    }

    functionObject[kJsonDescriptionKey] = descriptionMatch.captured(1).trimmed();

    QJsonObject parameters;
    parameters[kJsonTypeKey] = "object";

    QJsonObject properties;
    QJsonArray required;

    QRegularExpressionMatchIterator paramMatches = paramRegex.globalMatch(docstring);
    while (paramMatches.hasNext())
    {
        QRegularExpressionMatch paramMatch = paramMatches.next();
        if (paramMatch.hasMatch())
        {
            QString paramName = paramMatch.captured(1).trimmed();
            QString paramType = paramMatch.captured(2).trimmed();
            QString paramDescription = paramMatch.captured(3).trimmed();

            QJsonObject paramObject;
            paramObject[kJsonTypeKey] = paramType;
            paramObject[kJsonDescriptionKey] = paramDescription;

            properties[paramName] = paramObject;
            required.append(paramName);
        }
    }

    if (!properties.isEmpty())
    {
        parameters["properties"] = properties;
        parameters["required"] = required;
    }
    parameters["additionalProperties"] = false;

    functionObject[kJsonParametersKey] = parameters;

    QRegularExpressionMatch returnMatch = returnRegex.match(docstring);
    if (returnMatch.hasMatch())
    {
        QJsonObject returnObject;
        returnObject[kJsonDescriptionKey] = returnMatch.captured(1).trimmed();
        functionObject[kJsonReturnKey] = returnObject;
    }

    result[kJsonFunctionKey] = functionObject;

    return result;
}

}

QString getNameFromJson(QJsonObject json)
{
    if (json.contains(kJsonFunctionKey) && json[kJsonFunctionKey].isObject())
    {
        QJsonObject functionObject = json[kJsonFunctionKey].toObject();
        if (functionObject.contains(kJsonNameKey) && functionObject[kJsonNameKey].isString())
        {
            return functionObject[kJsonNameKey].toString();
        }
    }
    return "";
}

Agent::Agent(const QString& model, const QString& systemPrompt)
    : m_model(model)
    , m_systemPrompt(systemPrompt)
{
    clear();
}

void Agent::setErrorCallback(ErrorReceivedCallback callback) { m_errorCallback = std::move(callback); }

void Agent::addInitialMessage(const QString& message)
{
    m_messages.append(Message { .role = "assistant", .content = message });
}

void Agent::requestAsync(const Client& client, const QString& message, const ResponseReceivedCallback& callback)
{
    if (m_responseReceivedCallback)
    {
        qCWarning(AgentLogic) << "Cannot request when previous request still processing";
        return;
    }

    m_messages.append(Message { .role = "user", .content = message });
    m_toolCallsCount = 0;

    m_responseReceivedCallback = callback;
    createCompletionAsync(client);
}

void Agent::clear()
{
    m_messages.clear();
    m_messages.append(Message { .role = "system", .content = m_systemPrompt });
}

const Messages& Agent::messages() const { return m_messages; }

void Agent::addTool(const QString docstring, const Tool& tool)
{
    auto json = parseDocstring(docstring);
    auto name = getNameFromJson(json);

    if (name.isEmpty())
    {

        qCWarning(AgentLogic) << QStringLiteral("Name is empty. Check docstring");
        return;
    }

    if (m_toolsMap.contains(name))
    {
        qCWarning(AgentLogic) << QStringLiteral("Tool %1 already exits. Will be replaced").arg(name);
    }

    QJsonDocument doc(json);
    qCDebug(AgentLogic) << doc.toJson();

    qCInfo(AgentLogic) << QStringLiteral("Adding tool %1").arg(name);
    m_toolsMap.insert(name, ToolData { .tool = tool, .json = json });
}

bool Agent::asynRequestInProgress() const { return m_responseReceivedCallback != nullptr; }

void Agent::createCompletionAsync(const Client& client)
{
    client.createCompletionAsync(m_model, m_messages, m_toolsMap,
                                 [this, &client](const Completion& completion)
                                 { onCompletionReceived(client, completion); });
}

void Agent::deliverResponse(const QString& response)
{
    if (m_responseReceivedCallback)
    {
        m_responseReceivedCallback(response);
        m_responseReceivedCallback = {};
    }
}

void Agent::onCompletionReceived(const Client& client, const Completion& completion)
{
    if (!completion.error.isEmpty() || completion.choices.size() == 0)
    {
        const QString error = completion.error.isEmpty()
            ? QStringLiteral("Empty response from the model")
            : completion.error;
        qCWarning(AgentLogic) << QStringLiteral("Completion failed:") << error;
        if (m_errorCallback)
        {
            m_errorCallback(error);
        }
        deliverResponse("");
        return;
    }

    auto& choice = completion.choices.at(0);
    if (choice.finish_reason == "tool_calls")
    {
        m_messages.append(choice.message);
        handleToolCallsAsync(client, choice.message.tool_calls, 0);
        return;
    }

    auto response = choice.message.content;
    m_messages.append(Message { .role = "assistant", .content = response });
    deliverResponse(response);
}

void Agent::handleToolCallsAsync(const Client& client, QVector<ToolCall> toolCalls, int index)
{
    if (index == 0 && ++m_toolCallsCount > kMaxToolCallsCount)
    {
        qCWarning(AgentLogic) << "Too many tool calls count";
        deliverResponse("");
        return;
    }

    if (index >= toolCalls.size())
    {
        if (m_responseReceivedCallback)
        {
            m_responseReceivedCallback("");
        }
        createCompletionAsync(client);
        return;
    }

    const ToolCall toolCall = toolCalls.at(index);
    qCInfo(AgentLogic) << "Tool call" << toolCall.name << toolCall.arguments << toolCall.id << toolCall.type;

    if (!m_toolsMap.contains(toolCall.name))
    {
        qCWarning(AgentLogic) << QStringLiteral("Tool %1 doesn't exit").arg(toolCall.name);
        handleToolCallsAsync(client, toolCalls, index + 1);
        return;
    }

    m_toolsMap[toolCall.name].tool(
        toolCall.arguments,
        [this, &client, toolCalls, index, id = toolCall.id](QVariant ret)
        {
            QVariantMap map;
            map["value"] = ret;
            QJsonDocument doc = QJsonDocument::fromVariant(map);

            m_messages.append(Message { .role = "tool",
                                        .content = doc.toJson(QJsonDocument::Compact),
                                        .tool_call_id = id });

            handleToolCallsAsync(client, toolCalls, index + 1);
        });
}
