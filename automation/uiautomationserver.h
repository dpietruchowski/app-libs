#pragma once

#include <QHash>
#include <QObject>

class QQmlApplicationEngine;
class QTcpServer;
class QTcpSocket;
class QJsonObject;

class UiAutomationServer : public QObject
{
    Q_OBJECT
public:
    UiAutomationServer(QQmlApplicationEngine* engine, quint16 port, QObject* parent = nullptr);

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    QJsonObject handleCommand(const QJsonObject& request);
    QObject* findByObjectName(const QString& name) const;

    QQmlApplicationEngine* m_engine;
    QTcpServer* m_server;
    QHash<QTcpSocket*, QByteArray> m_buffers;
};
