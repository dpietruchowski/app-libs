#pragma once

#include <QString>
#include <functional>
#include <vector>

#include "agent/agent.h"

namespace chat
{

struct AppAgent final
{
    static constexpr int kDefaultMaxDelegations = 3;

    QString key;
    QString title;
    QString description;
    ::Agent agent;
    std::function<QString()> followUp;
    ModelConfig config;
    int maxDelegations { kDefaultMaxDelegations };

    QString modelFor(const QString& chosen, const QString& chatModel) const;
};

class AgentCatalog final
{
public:
    bool add(AppAgent agent);

    const AppAgent* find(const QString& agentKey) const;
    const std::vector<AppAgent>& agents() const { return m_agents; }

private:
    std::vector<AppAgent> m_agents;
};

}
