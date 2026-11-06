#include "mcpdispatcher.h"

#include <utility>

namespace
{

const QString kJsonRpcVersion = QStringLiteral("2.0");
const QString kDefaultProtocolVersion = QStringLiteral("2025-11-25");
const QString kServerName = QStringLiteral("app");
const QString kServerVersion = QStringLiteral("1");
const QString kNotificationPrefix = QStringLiteral("notifications/");
const QString kInitialize = QStringLiteral("initialize");
const QString kToolsList = QStringLiteral("tools/list");
const QString kToolsCall = QStringLiteral("tools/call");

constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;

QJsonObject envelope(const QJsonValue& id)
{
    QJsonObject object { { "jsonrpc", kJsonRpcVersion } };
    if (!id.isUndefined())
        object.insert("id", id);
    return object;
}

McpAction replyWith(QJsonObject reply)
{
    McpAction action;
    action.reply = std::move(reply);
    return action;
}

McpAction resultReply(const QJsonValue& id, const QJsonObject& result)
{
    QJsonObject reply = envelope(id);
    reply.insert("result", result);
    return replyWith(std::move(reply));
}

McpAction errorReply(const QJsonValue& id, int code, const QString& message)
{
    QJsonObject reply = envelope(id.isUndefined() ? QJsonValue(QJsonValue::Null) : id);
    reply.insert("error", QJsonObject { { "code", code }, { "message", message } });
    return replyWith(std::move(reply));
}

QJsonObject toolDescription(const QString& name, const QJsonObject& function)
{
    return { { "name", name },
             { "description", function.value("description") },
             { "inputSchema", function.value("parameters") } };
}

QJsonObject initializeResult(const QJsonObject& message)
{
    const QString requested
        = message.value("params").toObject().value("protocolVersion").toString();
    return { { "protocolVersion", requested.isEmpty() ? kDefaultProtocolVersion : requested },
             { "capabilities", QJsonObject { { "tools", QJsonObject() } } },
             { "serverInfo",
               QJsonObject { { "name", kServerName }, { "version", kServerVersion } } } };
}

}

McpDispatcher::McpDispatcher(const ToolsMap& toolsMap)
{
    for (auto it = toolsMap.constBegin(); it != toolsMap.constEnd(); ++it)
    {
        m_toolNames.insert(it.key());
        m_tools.append(toolDescription(it.key(), it.value().json.value("function").toObject()));
    }
}

McpAction McpDispatcher::handle(const QString& requestId, const QJsonValue& message) const
{
    if (!message.isObject())
        return errorReply(QJsonValue(QJsonValue::Null), kInvalidRequest, "Invalid Request");

    const QJsonObject object = message.toObject();
    const QJsonValue id = object.value("id");
    const QString method = object.value("method").toString();

    if (method == kInitialize)
        return resultReply(id, initializeResult(object));
    if (method.startsWith(kNotificationPrefix))
        return resultReply(QJsonValue(QJsonValue::Undefined), QJsonObject());
    if (method == kToolsList)
        return resultReply(id, QJsonObject { { "tools", m_tools } });
    if (method == kToolsCall)
        return replyToToolCall(requestId, object);
    return errorReply(id, kMethodNotFound, "Method not found");
}

McpAction McpDispatcher::replyToToolCall(const QString& requestId, const QJsonObject& message) const
{
    const QJsonValue id = message.value("id");
    const QJsonObject params = message.value("params").toObject();
    const QString name = params.value("name").toString();

    if (!m_toolNames.contains(name))
    {
        return replyWith(toolResponse(id, QStringLiteral("Unknown tool: %1").arg(name), true));
    }

    McpAction action;
    action.kind = McpAction::Kind::CallTool;
    action.rpcId = id;
    action.toolCall.id = requestId;
    action.toolCall.name = name;
    action.toolCall.arguments = params.value("arguments").toObject().toVariantMap();
    action.toolCall.type = QStringLiteral("function");
    return action;
}

QJsonObject McpDispatcher::toolResponse(const QJsonValue& rpcId, const QString& text, bool isError,
                                        const QVector<ToolImage>& images)
{
    QJsonArray content { QJsonObject { { "type", "text" }, { "text", text } } };
    for (const ToolImage& image : images)
    {
        content.append(QJsonObject { { "type", "image" },
                                     { "data", QString::fromLatin1(image.data.toBase64()) },
                                     { "mimeType", image.mimeType } });
    }

    QJsonObject reply = envelope(rpcId);
    reply.insert("result", QJsonObject { { "content", content }, { "isError", isError } });
    return reply;
}
