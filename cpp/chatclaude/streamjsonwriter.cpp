#include "chatclaude/streamjsonwriter.h"

#include <QJsonDocument>
#include <QJsonValue>

namespace chat::claude
{

namespace
{

    QByteArray lineOf(const QJsonObject& object)
    {
        return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    }

}

QByteArray StreamJsonWriter::initialize()
{
    return lineOf({ { "type", "control_request" },
                    { "request_id", "init_1" },
                    { "request",
                      QJsonObject { { "subtype", "initialize" },
                                    { "hooks", QJsonValue(QJsonValue::Null) } } } });
}

QByteArray StreamJsonWriter::userMessage(const QString& text)
{
    return lineOf({ { "type", "user" },
                    { "message", QJsonObject { { "role", "user" }, { "content", text } } },
                    { "parent_tool_use_id", QJsonValue(QJsonValue::Null) },
                    { "session_id", "default" } });
}

QByteArray StreamJsonWriter::mcpResponse(const QString& requestId, const QJsonObject& rpcResponse)
{
    return lineOf(
        { { "type", "control_response" },
          { "response",
            QJsonObject { { "subtype", "success" },
                          { "request_id", requestId },
                          { "response", QJsonObject { { "mcp_response", rpcResponse } } } } } });
}

}
