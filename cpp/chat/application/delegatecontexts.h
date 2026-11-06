#pragma once

#include <QDateTime>
#include <QString>
#include <QUuid>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "agent/agent.h"
#include "chat/application/agentcatalog.h"

namespace chat
{

struct DelegateContext final
{
    std::unique_ptr<::Agent> agent;
    QString session;
    int inputTokens { 0 };
    QDateTime requestStarted;

    bool exceeds(const ContextPolicy& policy) const { return inputTokens > policy.maxInputTokens; }
};

class DelegateContexts final
{
public:
    using Clock = std::function<QDateTime()>;
    using ForgetSession = std::function<void(const QString& session)>;

    DelegateContexts(Clock clock, ForgetSession forget);
    ~DelegateContexts();

    DelegateContexts(const DelegateContexts&) = delete;
    DelegateContexts& operator=(const DelegateContexts&) = delete;

    QDateTime now() const;
    std::optional<DelegateContext> take(const QUuid& conversationId, const QString& agentKey,
                                        const ContextPolicy& policy);
    void keep(const QUuid& conversationId, const QString& agentKey, DelegateContext context);
    void forgetSession(const QString& session) const;
    void forgetConversation(const QUuid& conversationId);

private:
    using Key = std::pair<QUuid, QString>;

    bool tooOld(const DelegateContext& context, const ContextPolicy& policy) const;

    Clock m_clock;
    ForgetSession m_forget;
    std::map<Key, DelegateContext> m_contexts;
};

}
