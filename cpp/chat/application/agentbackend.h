#pragma once

#include <QHash>
#include <QString>
#include <QUuid>
#include <memory>
#include <optional>
#include <vector>

#include "chat/application/agentcatalog.h"
#include "chat/application/agentturn.h"
#include "chat/domain/chatcontext.h"
#include "chat/domain/message.h"

namespace chat
{

struct TurnRequest final
{
    QUuid conversationId;
    QString mainInstructions;
    QString model;
    QString reasoningEffort;
    QHash<QString, QString> agentModels;
    std::vector<Message> transcript;
    std::optional<ChatContext> context;
    QString backendSession;
};

class AgentBackend
{
public:
    virtual ~AgentBackend() = default;

    virtual bool available() const = 0;
    virtual std::unique_ptr<AgentTurn> start(const TurnRequest& request,
                                             const AgentCatalog& catalog)
        = 0;
    virtual void forget(const QString& backendSession) = 0;
};

}
