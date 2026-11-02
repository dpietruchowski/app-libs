#include "anthropicclient.h"

#include <QEventLoop>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "logging.h"

namespace
{
const char* kApiVersion = "2026-10-21";
const int kMaxTokens = 16000;

QJsonObject cacheControl() { return QJsonObject { { "type", "ephemeral" }, { "ttl", "1h" } }; }

QJsonObject textBlock(const QString& text)
{
    return QJsonObject { { "type", "text" }, { "text", text } };
}

QString effortFor(const QString& reasoningEffort)
{
    static const QHash<QString, QString> kApiEffort { { "minimal", "low" } };
    const QString effort = reasoningEffort.toLower();
    return kApiEffort.value(effort, effort);
}

QJsonArray systemBlocks(const Messages& messages)
{
    QJsonArray blocks;
    for (const Message& message : messages)
    {
        if (message.role == QLatin1String("system"))
        {
            blocks.append(textBlock(message.content));
        }
    }
    if (!blocks.isEmpty())
    {
        QJsonObject last = blocks.takeAt(blocks.size() - 1).toObject();
        last["cache_control"] = cacheControl();
        blocks.append(last);
    }
    return blocks;
}

QJsonArray toolDefinitions(const ToolsMap& toolsMap)
{
    QJsonArray tools;
    for (const ToolData& toolData : toolsMap)
    {
        const QJsonObject function = toolData.json.value("function").toObject();
        tools.append(QJsonObject { { "name", function.value("name") },
                                   { "description", function.value("description") },
                                   { "input_schema", function.value("parameters") } });
    }
    return tools;
}

QJsonArray assistantBlocks(const Message& message)
{
    if (!message.raw_content.isEmpty())
    {
        return message.raw_content;
    }

    QJsonArray blocks;
    if (!message.content.isEmpty())
    {
        blocks.append(textBlock(message.content));
    }
    for (const ToolCall& toolCall : message.tool_calls)
    {
        blocks.append(QJsonObject { { "type", "tool_use" },
                                    { "id", toolCall.id },
                                    { "name", toolCall.name },
                                    { "input", QJsonObject::fromVariantMap(toolCall.arguments) } });
    }
    return blocks;
}

QJsonObject imageBlock(const ToolImage& image)
{
    const QJsonObject source { { "type", "base64" },
                               { "media_type", image.mimeType },
                               { "data", QString::fromLatin1(image.data.toBase64()) } };
    return QJsonObject { { "type", "image" }, { "source", source } };
}

QJsonObject toolResultBlock(const Message& message)
{
    QJsonValue content = message.content;
    if (!message.images.isEmpty())
    {
        QJsonArray blocks { textBlock(message.content) };
        for (const ToolImage& image : message.images)
        {
            blocks.append(imageBlock(image));
        }
        content = blocks;
    }
    return QJsonObject { { "type", "tool_result" },
                         { "tool_use_id", message.tool_call_id },
                         { "content", content } };
}

QJsonObject turn(const QString& role, const QJsonArray& content)
{
    return QJsonObject { { "role", role }, { "content", content } };
}

QJsonArray conversation(const Messages& messages)
{
    QJsonArray turns;
    QJsonArray toolResults;
    const auto flushToolResults = [&]
    {
        if (!toolResults.isEmpty())
        {
            turns.append(turn("user", toolResults));
            toolResults = QJsonArray();
        }
    };

    for (const Message& message : messages)
    {
        if (message.role == QLatin1String("system"))
        {
            continue;
        }
        if (message.role == QLatin1String("tool"))
        {
            toolResults.append(toolResultBlock(message));
            continue;
        }

        flushToolResults();
        const bool assistant = message.role == QLatin1String("assistant");
        QJsonArray blocks;
        if (assistant)
        {
            blocks = assistantBlocks(message);
        }
        else if (!message.content.isEmpty())
        {
            blocks.append(textBlock(message.content));
        }
        if (!blocks.isEmpty())
        {
            turns.append(turn(assistant ? "assistant" : "user", blocks));
        }
    }
    flushToolResults();
    return turns;
}

QString errorToken(const QString& errorType)
{
    static const QHash<QString, QString> kTokens {
        { "authentication_error", "invalid_key" },
        { "permission_error", "invalid_key" },
        { "billing_error", "insufficient_credits" },
        { "rate_limit_error", "rate_limit" },
        { "api_error", "server_error" },
        { "overloaded_error", "server_error" },
        { "timeout_error", "server_error" },
    };
    return kTokens.value(errorType, QStringLiteral("unknown_error"));
}

QString finishReasonFor(const QString& stopReason)
{
    static const QHash<QString, QString> kFinishReasons {
        { "tool_use", "tool_calls" },
        { "end_turn", "stop" },
        { "stop_sequence", "stop" },
        { "max_tokens", "length" },
    };
    return kFinishReasons.value(stopReason);
}

Usage usageFrom(const QJsonObject& usage)
{
    const int input = usage.value("input_tokens").toInt();
    const int cacheWrite = usage.value("cache_creation_input_tokens").toInt();
    const int cacheRead = usage.value("cache_read_input_tokens").toInt();
    const int output = usage.value("output_tokens").toInt();

    Usage result;
    result.prompt_tokens = input + cacheWrite + cacheRead;
    result.completion_tokens = output;
    result.total_tokens = result.prompt_tokens + output;
    result.context_tokens = result.prompt_tokens;
    return result;
}

void logUsage(const QJsonObject& usage)
{
    qCInfo(ClientLogic) << "anthropic usage: input" << usage.value("input_tokens").toInt()
                        << "cache read" << usage.value("cache_read_input_tokens").toInt()
                        << "cache write" << usage.value("cache_creation_input_tokens").toInt()
                        << "output" << usage.value("output_tokens").toInt();
}
}

namespace anthropic
{
QJsonObject requestBody(const ModelConfig& config, const Messages& messages,
                        const ToolsMap& toolsMap)
{
    QJsonObject body;
    body["model"] = config.model;
    body["max_tokens"] = kMaxTokens;
    body["cache_control"] = cacheControl();

    const QJsonArray system = systemBlocks(messages);
    if (!system.isEmpty())
    {
        body["system"] = system;
    }

    const QJsonArray tools = toolDefinitions(toolsMap);
    if (!tools.isEmpty())
    {
        body["tools"] = tools;
    }

    body["messages"] = conversation(messages);

    if (!config.reasoningEffort.isEmpty())
    {
        body["output_config"] = QJsonObject { { "effort", effortFor(config.reasoningEffort) } };
    }
    return body;
}

Completion completionFrom(const QJsonObject& response)
{
    Completion completion {};
    if (response.value("type").toString() == QLatin1String("error"))
    {
        completion.error = errorToken(response.value("error").toObject().value("type").toString());
        return completion;
    }

    completion.id = response.value("id").toString();
    completion.object = response.value("type").toString();
    completion.model = response.value("model").toString();
    completion.usage = usageFrom(response.value("usage").toObject());

    const QString stopReason = response.value("stop_reason").toString();
    if (stopReason == QLatin1String("refusal"))
    {
        completion.error = QStringLiteral("refusal");
        return completion;
    }
    const QString finishReason = finishReasonFor(stopReason);
    if (finishReason.isEmpty())
    {
        completion.error = QStringLiteral("unknown_error");
        return completion;
    }

    Message message;
    message.role = QStringLiteral("assistant");
    message.raw_content = response.value("content").toArray();
    for (const QJsonValue& value : message.raw_content)
    {
        const QJsonObject block = value.toObject();
        const QString type = block.value("type").toString();
        if (type == QLatin1String("text"))
        {
            message.content += block.value("text").toString();
        }
        else if (type == QLatin1String("tool_use"))
        {
            message.tool_calls.append(
                ToolCall { .id = block.value("id").toString(),
                           .name = block.value("name").toString(),
                           .arguments = block.value("input").toObject().toVariantMap(),
                           .type = QStringLiteral("function") });
        }
    }

    Choice choice;
    choice.message = message;
    choice.finish_reason = finishReason;
    choice.index = 0;
    completion.choices.append(choice);
    return completion;
}
}

AnthropicClient::AnthropicClient(const QString& apiKey, const QString& url)
    : Client(url, apiKey)
{
}

void AnthropicClient::createCompletionAsync(const ModelConfig& config, const Messages& messages,
                                            const ToolsMap& toolsMap,
                                            const CompletionCreatedCallback& callback) const
{
    QNetworkReply* reply = post(config, messages, toolsMap);
    if (!reply)
    {
        return;
    }
    QObject::connect(reply, &QNetworkReply::finished, reply,
                     [this, reply, callback]
                     {
                         Completion completion = parse(reply);
                         callback(completion);
                     });
}

void AnthropicClient::createCompletionAsync(const ModelConfig& config, const Messages& messages,
                                            const ToolsMap& toolsMap,
                                            const CompletionCreatedCallback& callback,
                                            const PartialTextCallback&) const
{
    createCompletionAsync(config, messages, toolsMap, callback);
}

Completion AnthropicClient::createCompletion(const ModelConfig& config, const Messages& messages,
                                             const ToolsMap& toolsMap) const
{
    QNetworkReply* reply = post(config, messages, toolsMap);
    if (!reply)
    {
        return Completion { .error = QStringLiteral("invalid_key") };
    }
    QEventLoop eventLoop;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();
    return parse(reply);
}

QNetworkReply* AnthropicClient::post(const ModelConfig& config, const Messages& messages,
                                     const ToolsMap& toolsMap) const
{
    if (m_apiKey.isEmpty())
    {
        qCWarning(ClientLogic) << "Cannot make request with empty api key";
        return nullptr;
    }

    qCInfo(ClientLogic) << "POST" << m_url << "model:" << config.model;

    QNetworkRequest request(QUrl { m_url });
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", m_apiKey.toUtf8());
    request.setRawHeader("anthropic-version", kApiVersion);

    const QJsonObject body = anthropic::requestBody(config, messages, toolsMap);
    return manager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

Completion AnthropicClient::parse(QNetworkReply* reply) const
{
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool failed = reply->error() != QNetworkReply::NoError;
    const QByteArray body = reply->readAll();
    const QString errorString = reply->errorString();
    delete reply;

    const QJsonObject json = QJsonDocument::fromJson(body).object();
    const bool typedError = json.value("type").toString() == QLatin1String("error");
    const Completion completion = failed && !typedError
        ? Completion { .error = classifyError(httpStatus, body) }
        : anthropic::completionFrom(json);

    if (json.contains("usage"))
    {
        logUsage(json.value("usage").toObject());
    }
    if (!completion.error.isEmpty())
    {
        qCWarning(ClientLogic) << "Error occurred:" << errorString << "HTTP status:" << httpStatus
                               << "Server replied:" << QString(body);
    }
    return completion;
}
