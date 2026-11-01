#include "chat/application/agentcatalog.h"

#include <algorithm>
#include <utility>

namespace chat
{

namespace
{

    Task<QJsonValue> failure(const QString& message)
    {
        return Task<QJsonValue>::ready(nullptr, Result<QJsonValue>::failure(message));
    }

}

void AgentCatalog::add(AgentDefinition agent)
{
    const auto existing
        = std::find_if(m_agents.begin(), m_agents.end(), [&agent](const AgentDefinition& candidate)
                       { return candidate.key == agent.key; });
    if (existing == m_agents.end())
    {
        m_agents.push_back(std::move(agent));
        return;
    }
    *existing = std::move(agent);
}

const AgentDefinition* AgentCatalog::find(const QString& agentKey) const
{
    const auto found = std::find_if(m_agents.begin(), m_agents.end(),
                                    [&agentKey](const AgentDefinition& candidate)
                                    { return candidate.key == agentKey; });
    return found == m_agents.end() ? nullptr : &*found;
}

Task<QJsonValue> AgentCatalog::invoke(const QString& agentKey, const QString& toolName,
                                      const QJsonObject& arguments) const
{
    const AgentDefinition* agent = find(agentKey);
    const ToolSpec* tool = agent == nullptr ? nullptr : agent->findTool(toolName);
    if (tool == nullptr)
    {
        return failure(QStringLiteral("Unknown tool %1 of agent %2").arg(toolName, agentKey));
    }
    if (!tool->handler())
    {
        return failure(
            QStringLiteral("Tool %1 of agent %2 has no handler").arg(toolName, agentKey));
    }
    return tool->handler()(arguments);
}

}
