#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include <memory>
#include <optional>
#include <vector>

#include "chat/application/agentcatalog.h"
#include "chat/domain/chatcontext.h"
#include "chat/domain/message.h"
#include "chat/domain/step.h"

namespace chat
{

namespace turn_error
{

    inline const QString kNetworkError = QStringLiteral("network_error");
    inline const QString kServerError = QStringLiteral("server_error");
    inline const QString kCancelled = QStringLiteral("cancelled");
    inline const QString kTimeout = QStringLiteral("timeout");
    inline const QString kUnavailable = QStringLiteral("unavailable");
    inline const QString kBridgeUnavailable = QStringLiteral("bridge_unavailable");

}

struct TurnRequest final
{
    QUuid conversationId;
    QString mainInstructions;
    QString model;
    QString reasoningEffort;
    std::vector<Message> transcript;
    std::optional<ChatContext> context;
};

class AgentTurn : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void cancel() = 0;

signals:
    void stepStarted(const chat::Step& step);
    void stepFinished(const chat::Step& step);
    void finished(const QString& answer);
    void failed(const QString& errorToken);
};

class AgentBackend
{
public:
    virtual ~AgentBackend() = default;

    virtual bool available() const = 0;
    virtual std::unique_ptr<AgentTurn> start(const TurnRequest& request,
                                             const AgentCatalog& catalog)
        = 0;
};

}
