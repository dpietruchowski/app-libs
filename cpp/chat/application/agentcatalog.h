#pragma once

#include <QString>
#include <chrono>
#include <functional>
#include <optional>
#include <vector>

#include "agent/agent.h"

namespace chat
{

struct ContextPolicy final
{
    int maxInputTokens { 0 };
    std::chrono::milliseconds maxAge { 0 };
    QString compactionRequest;
    std::function<QString(const QString& reply)> compactedContext;

    bool compacts() const { return !compactionRequest.isEmpty() && compactedContext; }
};

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
    std::optional<ContextPolicy> keepContext;

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
