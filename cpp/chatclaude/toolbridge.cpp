#include "chatclaude/toolbridge.h"

#include <QHostAddress>
#include <QMetaObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <optional>

namespace chat::claude
{

namespace
{

    QByteArray generateToken()
    {
        quint32 words[8];
        QRandomGenerator::system()->fillRange(words);
        return QByteArray(reinterpret_cast<const char*>(words), sizeof(words)).toHex();
    }

    int statusFor(HttpParseError error)
    {
        switch (error)
        {
            case HttpParseError::BodyTooLarge:
                return 413;
            case HttpParseError::HeadersTooLarge:
                return 431;
            default:
                return 400;
        }
    }

    HttpResponse internalServerError()
    {
        HttpResponse response;
        response.status = 500;
        return response;
    }

}

ToolBridge::ToolBridge(const AgentCatalog& catalog, QObject* parent)
    : QObject(parent)
    , m_token(generateToken())
    , m_dispatcher(catalog, m_token)
    , m_server(this)
{
    connect(&m_server, &QTcpServer::newConnection, this, [this] { acceptPending(); });
}

bool ToolBridge::start() { return m_server.listen(QHostAddress::LocalHost, 0); }

void ToolBridge::stop()
{
    m_server.close();
    m_connections.clear();
    for (QTcpSocket* socket : m_server.findChildren<QTcpSocket*>(Qt::FindDirectChildrenOnly))
    {
        socket->abort();
        socket->deleteLater();
    }
}

bool ToolBridge::isListening() const { return m_server.isListening(); }

quint16 ToolBridge::port() const { return m_server.serverPort(); }

const QByteArray& ToolBridge::token() const { return m_token; }

QString ToolBridge::agentUrl(const QString& agentKey) const
{
    return QStringLiteral("http://127.0.0.1:%1/mcp/%2").arg(port()).arg(agentKey);
}

void ToolBridge::acceptPending()
{
    while (QTcpSocket* socket = m_server.nextPendingConnection())
    {
        m_connections.insert(socket, Connection {});
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { receive(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] { drop(socket); });
    }
}

void ToolBridge::receive(QTcpSocket* socket)
{
    const auto connection = m_connections.find(socket);
    if (connection == m_connections.end())
        return;

    connection->parser.feed(socket->readAll());
    serve(socket);
}

void ToolBridge::serve(QTcpSocket* socket)
{
    const auto connection = m_connections.find(socket);
    if (connection == m_connections.end() || connection->busy)
        return;

    const std::optional<HttpRequest> request = connection->parser.takeRequest();
    if (!request)
    {
        if (connection->parser.failed())
            reject(socket, connection->parser.error());
        return;
    }

    connection->busy = true;
    m_dispatcher.dispatch(*request).finally(
        this,
        [this, guard = QPointer<QTcpSocket>(socket)](Result<HttpResponse> result)
        {
            QTcpSocket* replyTo = guard.data();
            if (!replyTo)
                return;
            const auto pending = m_connections.find(replyTo);
            if (pending == m_connections.end())
                return;

            pending->busy = false;
            replyTo->write((result.isSuccess() ? result.value() : internalServerError()).toBytes());
            QMetaObject::invokeMethod(
                this,
                [this, guard]
                {
                    if (QTcpSocket* next = guard.data())
                        serve(next);
                },
                Qt::QueuedConnection);
        });
}

void ToolBridge::reject(QTcpSocket* socket, HttpParseError error)
{
    HttpResponse response;
    response.status = statusFor(error);
    response.headers.append(HttpHeader { "Connection", "close" });

    m_connections.remove(socket);
    socket->write(response.toBytes());
    socket->disconnectFromHost();
}

void ToolBridge::drop(QTcpSocket* socket)
{
    m_connections.remove(socket);
    socket->deleteLater();
}

}
