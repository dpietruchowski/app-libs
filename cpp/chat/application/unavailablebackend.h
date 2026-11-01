#pragma once

#include <memory>

#include "chat/application/agentbackend.h"

namespace chat
{

class UnavailableBackend final : public AgentBackend
{
public:
    bool available() const override;
    std::unique_ptr<AgentTurn> start(const TurnRequest& request,
                                     const AgentCatalog& catalog) override;
    void forget(const QString& backendSession) override;
};

}
