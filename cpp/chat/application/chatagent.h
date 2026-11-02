#pragma once

#include <QDateTime>
#include <QString>
#include <QUuid>
#include <functional>
#include <memory>

#include "agent/claudesession.h"
#include "agent/client.h"
#include "chat/application/agentbackend.h"

namespace chat
{

class DelegateContexts;

using ClientFactory = std::function<std::unique_ptr<::Client>(const ClaudeSession& session)>;

class ChatAgent final : public AgentBackend
{
public:
    using Clock = std::function<QDateTime()>;

    ChatAgent(ClientFactory clients, std::function<bool()> available,
              std::function<void(const QString&)> forget, Clock clock = {});

    bool available() const override;
    std::unique_ptr<AgentTurn> start(const TurnRequest& request,
                                     const AgentCatalog& catalog) override;
    void forget(const QString& backendSession) override;
    void forgetConversation(const QUuid& conversationId) override;

private:
    ClientFactory m_clients;
    std::function<bool()> m_available;
    std::function<void(const QString&)> m_forget;
    std::shared_ptr<DelegateContexts> m_contexts;
};

}
