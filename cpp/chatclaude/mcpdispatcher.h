#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>

#include "async/task.h"
#include "chat/application/agentcatalog.h"
#include "chat/domain/agentdefinition.h"
#include "chatclaude/httprequestparser.h"
#include "chatclaude/httpresponse.h"

namespace chat::claude
{

class McpDispatcher final : public QObject
{
public:
    McpDispatcher(const AgentCatalog& catalog, QByteArray token, QObject* parent = nullptr);

    Task<HttpResponse> dispatch(const HttpRequest& request);

private:
    Task<HttpResponse> handleMessage(const AgentDefinition& agent, const QJsonObject& message);
    Task<HttpResponse> callTool(const AgentDefinition& agent, const QJsonValue& id,
                                const QJsonObject& params);

    const AgentCatalog& m_catalog;
    QByteArray m_token;
};

}
