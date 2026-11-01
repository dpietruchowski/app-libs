#pragma once

#include <QString>
#include <functional>
#include <memory>

#include "agent/claudesession.h"
#include "agent/client.h"
#include "chat/application/agentbackend.h"

namespace chat
{

using ClientFactory = std::function<std::unique_ptr<::Client>(const ClaudeSession& session)>;

class ChatAgent final : public AgentBackend
{
public:
    ChatAgent(ClientFactory clients, std::function<bool()> available,
              std::function<void(const QString&)> forget);

    bool available() const override;
    std::unique_ptr<AgentTurn> start(const TurnRequest& request,
                                     const AgentCatalog& catalog) override;
    void forget(const QString& backendSession) override;

private:
    ClientFactory m_clients;
    std::function<bool()> m_available;
    std::function<void(const QString&)> m_forget;
};

}
