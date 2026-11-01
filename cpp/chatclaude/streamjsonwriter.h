#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace chat::claude
{

class StreamJsonWriter final
{
public:
    static QByteArray initialize();
    static QByteArray userMessage(const QString& text);
    static QByteArray mcpResponse(const QString& requestId, const QJsonObject& rpcResponse);
};

}
