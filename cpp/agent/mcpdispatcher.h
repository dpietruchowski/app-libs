#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QString>

#include "structures.h"

struct McpAction
{
    enum class Kind
    {
        Reply,
        CallTool
    };

    Kind kind { Kind::Reply };
    QJsonObject reply;
    ToolCall toolCall;
    QJsonValue rpcId;
};

class McpDispatcher final
{
public:
    explicit McpDispatcher(const ToolsMap& toolsMap);

    McpAction handle(const QString& requestId, const QJsonValue& message) const;

    static QJsonObject toolResponse(const QJsonValue& rpcId, const QString& text,
                                    bool isError = false);

private:
    McpAction replyToToolCall(const QString& requestId, const QJsonObject& message) const;

    QJsonArray m_tools;
    QSet<QString> m_toolNames;
};
