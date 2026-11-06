#include "chat/application/delegatecontexts.h"

#include <chrono>

namespace chat
{

DelegateContexts::DelegateContexts(Clock clock, ForgetSession forget)
    : m_clock(clock ? std::move(clock) : Clock([] { return QDateTime::currentDateTimeUtc(); }))
    , m_forget(std::move(forget))
{
}

DelegateContexts::~DelegateContexts()
{
    for (const auto& entry : m_contexts)
    {
        forgetSession(entry.second.session);
    }
}

QDateTime DelegateContexts::now() const { return m_clock(); }

std::optional<DelegateContext> DelegateContexts::take(const QUuid& conversationId,
                                                      const QString& agentKey,
                                                      const ContextPolicy& policy)
{
    const auto found = m_contexts.find(Key { conversationId, agentKey });
    if (found == m_contexts.end())
    {
        return std::nullopt;
    }
    DelegateContext context = std::move(found->second);
    m_contexts.erase(found);
    if (tooOld(context, policy) || (context.exceeds(policy) && !policy.compacts()))
    {
        forgetSession(context.session);
        return std::nullopt;
    }
    return context;
}

void DelegateContexts::keep(const QUuid& conversationId, const QString& agentKey,
                            DelegateContext context)
{
    context.agent->setObserver(AgentObserver {});
    const Key key { conversationId, agentKey };
    const auto previous = m_contexts.find(key);
    if (previous != m_contexts.end())
    {
        forgetSession(previous->second.session);
        m_contexts.erase(previous);
    }
    m_contexts.emplace(key, std::move(context));
}

void DelegateContexts::forgetSession(const QString& session) const
{
    if (!session.isEmpty() && m_forget)
    {
        m_forget(session);
    }
}

void DelegateContexts::forgetConversation(const QUuid& conversationId)
{
    for (auto entry = m_contexts.begin(); entry != m_contexts.end();)
    {
        if (entry->first.first != conversationId)
        {
            ++entry;
            continue;
        }
        forgetSession(entry->second.session);
        entry = m_contexts.erase(entry);
    }
}

bool DelegateContexts::tooOld(const DelegateContext& context, const ContextPolicy& policy) const
{
    const std::chrono::milliseconds age(context.requestStarted.msecsTo(now()));
    return age > policy.maxAge;
}

}
