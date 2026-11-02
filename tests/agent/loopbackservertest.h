#pragma once

#include <gtest/gtest.h>

#include <QByteArray>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

class LoopbackServerTest : public ::testing::Test
{
protected:
    QTcpServer server;
    QByteArray responseBody;
    int responseStatus = 200;
    QByteArray lastRequest;
    int requestCount = 0;

    void SetUp() override
    {
        ASSERT_TRUE(server.listen(QHostAddress::LocalHost, 0));

        QObject::connect(&server, &QTcpServer::newConnection, &server,
                         [this]
                         {
                             QTcpSocket* socket = server.nextPendingConnection();
                             QObject::connect(socket, &QTcpSocket::readyRead, socket,
                                              [this, socket] { onReadyRead(socket); });
                         });
    }

    QString url(const QString& path) const
    {
        return QString("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(path);
    }

    QJsonObject requestJson() const
    {
        return QJsonDocument::fromJson(lastRequest.mid(lastRequest.indexOf("\r\n\r\n") + 4))
            .object();
    }

    bool requestHasHeader(const QByteArray& header) const
    {
        const QByteArray headers = lastRequest.left(lastRequest.indexOf("\r\n\r\n")).toLower();
        return headers.contains(header.toLower());
    }

private:
    static int contentLength(const QByteArray& headers)
    {
        for (const QByteArray& line : headers.split('\n'))
        {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.toLower().startsWith("content-length:"))
            {
                return trimmed.mid(trimmed.indexOf(':') + 1).trimmed().toInt();
            }
        }
        return 0;
    }

    void onReadyRead(QTcpSocket* socket)
    {
        lastRequest += socket->readAll();

        const int headerEnd = lastRequest.indexOf("\r\n\r\n");
        if (headerEnd < 0)
        {
            return;
        }
        const QByteArray headers = lastRequest.left(headerEnd);
        const QByteArray body = lastRequest.mid(headerEnd + 4);
        if (body.size() < contentLength(headers))
        {
            return;
        }

        ++requestCount;
        const QByteArray response = "HTTP/1.1 " + QByteArray::number(responseStatus)
            + " Status\r\nContent-Type: application/json\r\nContent-Length: "
            + QByteArray::number(responseBody.size()) + "\r\nConnection: close\r\n\r\n"
            + responseBody;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }
};
