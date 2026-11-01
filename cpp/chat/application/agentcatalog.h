#pragma once

#include <QString>
#include <functional>
#include <vector>

#include "agent/agent.h"

namespace chat
{

struct AppAgent final
{
    QString key;
    QString title;
    QString description;
    ::Agent agent;
    std::function<QString()> followUp;
    QString model;
    QString reasoningEffort;
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
