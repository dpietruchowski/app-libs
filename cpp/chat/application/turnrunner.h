#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "async/task.h"
#include "chat/application/agentbackend.h"
#include "chat/domain/backendsession.h"
#include "chat/domain/chatcontext.h"
#include "chat/domain/conversation.h"
#include "chat/domain/enums.h"
#include "chat/domain/message.h"
#include "chat/domain/step.h"

namespace chat
{

class ChatStore;

struct TurnProfile final
{
    QString mainInstructions;
    QString model;
    QString reasoningEffort;
    QHash<QString, QString> agentModels;
};

class TurnRunner : public QObject
{
    Q_OBJECT

public:
    TurnRunner(ChatStore& store, AgentBackend& backend, const AgentCatalog& catalog,
               QObject* parent = nullptr);

    void setProfile(const TurnProfile& profile);

    bool send(const QUuid& conversationId, const QString& text,
              const std::optional<ChatContext>& context = std::nullopt);
    bool retry(const QUuid& conversationId);
    void cancel(const QUuid& conversationId);
    void remove(const QUuid& conversationId);
    void forgetSessions(const QStringList& backendSessions);

    bool isBusy(const QUuid& conversationId) const;

signals:
    void turnStarted(const QUuid& conversationId);
    void partialText(const QUuid& conversationId, const QString& text);
    void stepStarted(const QUuid& conversationId, const chat::Step& step);
    void stepFinished(const QUuid& conversationId, const chat::Step& step);
    void turnFinished(const QUuid& conversationId);
    void conversationRemoved(const QUuid& conversationId);

private:
    struct ActiveTurn final
    {
        QUuid assistantMessageId;
        std::unique_ptr<AgentTurn> turn;
        std::optional<BackendSession> previousSession;
        int seedStart { 0 };
        bool resumed { false };
        QString readySession;
        bool cancelRequested { false };

        std::optional<BackendSession> sessionAfterSuccess() const;
        QStringList knownSessions() const;
    };

    ActiveTurn* findTurn(const QUuid& conversationId);

    Task<void> launch(const QUuid& conversationId, const QUuid& assistantMessageId);
    void begin(const QUuid& conversationId, const QUuid& assistantMessageId,
               const Conversation& conversation, const std::vector<Message>& messages);
    void connectTurn(const QUuid& conversationId, AgentTurn& turn);
    void recordStep(const QUuid& conversationId, const Step& step);
    void conclude(const QUuid& conversationId, MessageStatus status, const QString& text,
                  const QString& error);
    void abandon(const QUuid& conversationId, const QString& reason);
    void finish(const QUuid& conversationId);
    void removeConversation(const QUuid& conversationId);
    void forgetAllBut(const QStringList& backendSessions,
                      const std::optional<BackendSession>& kept);

    ChatStore& m_store;
    AgentBackend& m_backend;
    const AgentCatalog& m_catalog;
    TurnProfile m_profile;
    std::map<QUuid, ActiveTurn> m_turns;
    std::set<QUuid> m_removing;
};

}
