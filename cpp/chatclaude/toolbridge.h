#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTcpServer>

#include "chat/application/agentcatalog.h"
#include "chatclaude/httprequestparser.h"
#include "chatclaude/httpresponse.h"
#include "chatclaude/mcpdispatcher.h"

class QTcpSocket;

namespace chat::claude
{

class ToolBridge final : public QObject
{
public:
    explicit ToolBridge(const AgentCatalog& catalog, QObject* parent = nullptr);

    bool start();
    void stop();

    bool isListening() const;
    quint16 port() const;
    const QByteArray& token() const;
    QString agentUrl(const QString& agentKey) const;

private:
    struct Connection
    {
        HttpRequestParser parser {};
        bool busy { false };
    };

    void acceptPending();
    void receive(QTcpSocket* socket);
    void serve(QTcpSocket* socket);
    void reject(QTcpSocket* socket, HttpParseError error);
    void drop(QTcpSocket* socket);

    QByteArray m_token;
    McpDispatcher m_dispatcher;
    QTcpServer m_server;
    QHash<QTcpSocket*, Connection> m_connections;
};

}
