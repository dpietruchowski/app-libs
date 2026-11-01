#include "chat/application/agentcatalog.h"

#include <QRegularExpression>
#include <algorithm>
#include <utility>

namespace chat
{

namespace
{

    bool isToolName(const QString& key)
    {
        static const QRegularExpression toolName(QStringLiteral("\\A\\w+\\z"));
        return toolName.match(key).hasMatch();
    }

}

bool AgentCatalog::add(AppAgent agent)
{
    if (!isToolName(agent.key) || agent.description.simplified().isEmpty() || find(agent.key))
    {
        return false;
    }
    m_agents.push_back(std::move(agent));
    return true;
}

const AppAgent* AgentCatalog::find(const QString& agentKey) const
{
    const auto found
        = std::find_if(m_agents.begin(), m_agents.end(), [&agentKey](const AppAgent& candidate)
                       { return candidate.key == agentKey; });
    return found == m_agents.end() ? nullptr : &*found;
}

}
