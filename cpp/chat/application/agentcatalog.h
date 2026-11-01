#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <vector>

#include "async/task.h"
#include "chat/domain/agentdefinition.h"

namespace chat
{

class AgentCatalog final
{
public:
    void add(AgentDefinition agent);

    const AgentDefinition* find(const QString& agentKey) const;
    const std::vector<AgentDefinition>& agents() const { return m_agents; }

    Task<QJsonValue> invoke(const QString& agentKey, const QString& toolName,
                            const QJsonObject& arguments) const;

private:
    std::vector<AgentDefinition> m_agents;
};

}
