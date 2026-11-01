#include "chatclaude/mcpdispatcher.h"

#include <QFuture>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QString>
#include <utility>

namespace chat::claude
{

namespace
{

    constexpr int kParseError = -32700;
    constexpr int kInvalidRequest = -32600;
    constexpr int kMethodNotFound = -32601;
    constexpr int kInvalidParams = -32602;

    constexpr QByteArrayView kAgentPathPrefix = "/mcp/";

    Task<HttpResponse> ready(const HttpResponse& response)
    {
        return Task<HttpResponse>::ready(nullptr, Result<HttpResponse>::success(response));
    }

    HttpResponse emptyResponse(int status)
    {
        HttpResponse response;
        response.status = status;
        return response;
    }

    HttpResponse jsonResponse(int status, const QJsonObject& body)
    {
        HttpResponse response;
        response.status = status;
        response.headers.append(HttpHeader { "Content-Type", "application/json" });
        response.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
        return response;
    }

    QJsonObject rpcEnvelope(const QJsonValue& id) { return { { "jsonrpc", "2.0" }, { "id", id } }; }

    HttpResponse rpcResult(const QJsonValue& id, const QJsonObject& result)
    {
        QJsonObject envelope = rpcEnvelope(id);
        envelope.insert("result", result);
        return jsonResponse(200, envelope);
    }

    HttpResponse rpcError(int status, const QJsonValue& id, int code, const QString& message)
    {
        QJsonObject envelope = rpcEnvelope(id);
        envelope.insert("error", QJsonObject { { "code", code }, { "message", message } });
        return jsonResponse(status, envelope);
    }

    QString encodeJson(const QJsonValue& value)
    {
        const QByteArray wrapped
            = QJsonDocument(QJsonArray { value }).toJson(QJsonDocument::Compact);
        return QString::fromUtf8(wrapped.sliced(1, wrapped.size() - 2));
    }

    QJsonObject toolContent(const QString& text, bool isError)
    {
        return { { "content", QJsonArray { QJsonObject { { "type", "text" }, { "text", text } } } },
                 { "isError", isError } };
    }

    QString textOfValue(const QJsonValue& value)
    {
        return value.isString() ? value.toString() : encodeJson(value);
    }

    QJsonObject toolResult(const Result<QJsonValue>& result)
    {
        return result.isSuccess() ? toolContent(textOfValue(result.value()), false)
                                  : toolContent(result.error(), true);
    }

    QJsonObject initializeResult(const QJsonObject& params)
    {
        const QString requested = params.value("protocolVersion").toString();
        return {
            { "protocolVersion", requested.isEmpty() ? QStringLiteral("2025-11-25") : requested },
            { "capabilities", QJsonObject { { "tools", QJsonObject() } } },
            { "serverInfo", QJsonObject { { "name", "chat-tool-bridge" }, { "version", "1" } } }
        };
    }

    QJsonObject toolsList(const AgentDefinition& agent)
    {
        QJsonArray tools;
        for (const ToolSpec& tool : agent.tools)
        {
            tools.append(QJsonObject { { "name", tool.name() },
                                       { "description", tool.description() },
                                       { "inputSchema", tool.inputSchema() } });
        }
        return { { "tools", tools } };
    }

    QString agentKeyOf(const QByteArray& path)
    {
        return path.startsWith(kAgentPathPrefix)
            ? QString::fromUtf8(path.sliced(kAgentPathPrefix.size()))
            : QString();
    }

}

McpDispatcher::McpDispatcher(const AgentCatalog& catalog, QByteArray token, QObject* parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_token(std::move(token))
{
}

Task<HttpResponse> McpDispatcher::dispatch(const HttpRequest& request)
{
    if (request.header("Authorization") != "Bearer " + m_token)
        return ready(emptyResponse(403));

    const AgentDefinition* agent = m_catalog.find(agentKeyOf(request.path));
    if (!agent)
        return ready(emptyResponse(404));

    if (request.method != "POST")
    {
        HttpResponse response = emptyResponse(405);
        response.headers.append(HttpHeader { "Allow", "POST" });
        return ready(response);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return ready(rpcError(400, QJsonValue(QJsonValue::Null), kParseError,
                              QStringLiteral("Parse error")));
    if (!document.isObject())
        return ready(rpcError(400, QJsonValue(QJsonValue::Null), kInvalidRequest,
                              QStringLiteral("Request must be a JSON object")));

    return handleMessage(*agent, document.object());
}

Task<HttpResponse> McpDispatcher::handleMessage(const AgentDefinition& agent,
                                                const QJsonObject& message)
{
    const QJsonValue id = message.value("id");
    if (id.isUndefined())
        return ready(emptyResponse(202));

    const QString method = message.value("method").toString();
    const QJsonObject params = message.value("params").toObject();

    if (method == QStringLiteral("initialize"))
        return ready(rpcResult(id, initializeResult(params)));
    if (method == QStringLiteral("ping"))
        return ready(rpcResult(id, QJsonObject()));
    if (method == QStringLiteral("tools/list"))
        return ready(rpcResult(id, toolsList(agent)));
    if (method == QStringLiteral("tools/call"))
        return callTool(agent, id, params);

    return ready(
        rpcError(200, id, kMethodNotFound, QStringLiteral("Method not found: %1").arg(method)));
}

Task<HttpResponse> McpDispatcher::callTool(const AgentDefinition& agent, const QJsonValue& id,
                                           const QJsonObject& params)
{
    const QJsonValue name = params.value("name");
    const QJsonValue arguments = params.value("arguments");
    if (!name.isString() || !(arguments.isUndefined() || arguments.isObject()))
        return ready(rpcError(200, id, kInvalidParams,
                              QStringLiteral("tools/call needs a tool name and object arguments")));

    Task<QJsonValue> invoked = m_catalog.invoke(agent.key, name.toString(), arguments.toObject());
    QFuture<Result<HttpResponse>> replied = invoked.future().then(
        this, [id](Result<QJsonValue> result)
        { return Result<HttpResponse>::success(rpcResult(id, toolResult(result))); });
    return Task<HttpResponse>(nullptr, std::move(replied));
}

}
