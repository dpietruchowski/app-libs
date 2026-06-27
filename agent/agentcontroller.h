#pragma once

#include "agent.h"
#include <QObject>

class AgentController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
public:
    AgentController(const QString& model, const QString& systemPrompt, QObject* parent = nullptr);

    void setModel(const QString& model);
    void setReasoningEffort(const QString& effort);
    void addTool(const QString docstring, const Tool& tool);

    Q_INVOKABLE void request(const Client& client, const QString& message);
    Q_INVOKABLE void clear(const QString& initMessage);

    QString lastResponse() const;

    QVariantList messages() const;
    bool isBusy() const;

signals:
    void messagesChanged();
    void busyChanged();
    void requestFinished();
    void errorOccurred(const QString& message);

private:
    Agent m_agent;
    QString m_lastResponse;
};
