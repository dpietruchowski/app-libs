#include "chat/application/turnrunner.h"

#include <QDebug>

#include "async/result.h"
#include "chat/application/chatstore.h"
#include "chat/application/transcript.h"

namespace chat
{

namespace
{

    const QString kNothingToRetry = QStringLiteral("The conversation has no failed turn to retry");

    std::optional<ChatContext> lastUserContext(const std::vector<Message>& messages)
    {
        for (auto message = messages.rbegin(); message != messages.rend(); ++message)
        {
            if (message->role() == Role::User)
            {
                return message->context();
            }
        }
        return std::nullopt;
    }

    bool isRetryable(const Message& message)
    {
        return message.role() == Role::Assistant
            && (message.status() == MessageStatus::Failed
                || message.status() == MessageStatus::Cancelled
                || message.status() == MessageStatus::Interrupted);
    }

    Result<QUuid> retryTarget(const std::vector<Message>& messages)
    {
        if (messages.empty() || !isRetryable(messages.back()))
        {
            return Result<QUuid>::failure(kNothingToRetry);
        }
        return Result<QUuid>::success(messages.back().id());
    }

}

std::optional<BackendSession> TurnRunner::ActiveTurn::sessionAfterSuccess() const
{
    const QString sessionId
        = !readySession.isEmpty() ? readySession : (resumed ? previousSession->id : QString());
    if (sessionId.isEmpty())
    {
        return std::nullopt;
    }
    return BackendSession { sessionId, resumed ? previousSession->start : seedStart };
}

QStringList TurnRunner::ActiveTurn::knownSessions() const
{
    QStringList known;
    if (previousSession)
    {
        known.append(previousSession->id);
    }
    if (!readySession.isEmpty() && !known.contains(readySession))
    {
        known.append(readySession);
    }
    return known;
}

TurnRunner::TurnRunner(ChatStore& store, AgentBackend& backend, const AgentCatalog& catalog,
                       QObject* parent)
    : QObject(parent)
    , m_store(store)
    , m_backend(backend)
    , m_catalog(catalog)
{
}

void TurnRunner::setProfile(const TurnProfile& profile) { m_profile = profile; }

bool TurnRunner::isBusy(const QUuid& conversationId) const
{
    return m_turns.count(conversationId) > 0 || m_removing.count(conversationId) > 0;
}

bool TurnRunner::send(const QUuid& conversationId, const QString& text,
                      const std::optional<ChatContext>& context)
{
    if (text.trimmed().isEmpty() || isBusy(conversationId))
    {
        return false;
    }
    m_turns.try_emplace(conversationId);

    Message::Data question;
    question.conversationId = conversationId;
    question.role = Role::User;
    question.text = text;
    question.status = MessageStatus::Complete;
    question.context = context;

    Message::Data answer;
    answer.conversationId = conversationId;
    answer.role = Role::Assistant;
    answer.status = MessageStatus::Running;

    m_store.appendMessage(question)
        .then(this, [this, answer](const Message&) { return m_store.appendMessage(answer); })
        .then(this, [this, conversationId](const Message& reply)
              { return launch(conversationId, reply.id()); })
        .finally(this,
                 [this, conversationId](const Result<void>& result)
                 {
                     if (result.isFailure())
                     {
                         abandon(conversationId, result.error());
                     }
                 });
    return true;
}

bool TurnRunner::retry(const QUuid& conversationId)
{
    if (isBusy(conversationId))
    {
        return false;
    }
    m_turns.try_emplace(conversationId);

    m_store.messages(conversationId)
        .then(this, [](const std::vector<Message>& messages) { return retryTarget(messages); })
        .then(this, [this](const QUuid& messageId) { return m_store.restartMessage(messageId); })
        .then(this, [this, conversationId](const Message& restarted)
              { return launch(conversationId, restarted.id()); })
        .finally(this,
                 [this, conversationId](const Result<void>& result)
                 {
                     if (result.isFailure())
                     {
                         abandon(conversationId, result.error());
                     }
                 });
    return true;
}

void TurnRunner::cancel(const QUuid& conversationId)
{
    ActiveTurn* active = findTurn(conversationId);
    if (!active)
    {
        return;
    }
    active->cancelRequested = true;
    if (active->turn)
    {
        active->turn->cancel();
    }
}

void TurnRunner::remove(const QUuid& conversationId)
{
    if (!m_removing.insert(conversationId).second)
    {
        return;
    }
    if (findTurn(conversationId))
    {
        cancel(conversationId);
        return;
    }
    removeConversation(conversationId);
}

void TurnRunner::forgetSessions(const QStringList& backendSessions)
{
    for (const QString& session : backendSessions)
    {
        m_backend.forget(session);
    }
}

TurnRunner::ActiveTurn* TurnRunner::findTurn(const QUuid& conversationId)
{
    const auto found = m_turns.find(conversationId);
    return found == m_turns.end() ? nullptr : &found->second;
}

Task<void> TurnRunner::launch(const QUuid& conversationId, const QUuid& assistantMessageId)
{
    return m_store.messages(conversationId)
        .then(this,
              [this, conversationId, assistantMessageId](const std::vector<Message>& messages)
              {
                  return m_store.conversation(conversationId)
                      .then(this,
                            [this, conversationId, assistantMessageId,
                             messages](const Conversation& conversation)
                            { begin(conversationId, assistantMessageId, conversation, messages); });
              });
}

void TurnRunner::begin(const QUuid& conversationId, const QUuid& assistantMessageId,
                       const Conversation& conversation, const std::vector<Message>& messages)
{
    ActiveTurn* active = findTurn(conversationId);
    if (!active)
    {
        return;
    }
    active->assistantMessageId = assistantMessageId;
    active->previousSession = conversation.backendSession();
    emit turnStarted(conversationId);

    if (active->cancelRequested)
    {
        conclude(conversationId, MessageStatus::Cancelled, QString(), turn_error::kCancelled);
        return;
    }

    const TurnTranscript transcript = planTranscript(messages, conversation.backendSession());
    active->resumed = transcript.resumed;
    active->seedStart = transcript.start;

    TurnRequest request;
    request.conversationId = conversationId;
    request.mainInstructions = m_profile.mainInstructions;
    request.model = m_profile.model;
    request.reasoningEffort = m_profile.reasoningEffort;
    request.transcript = transcript.messages;
    request.context = lastUserContext(messages);
    if (transcript.resumed)
    {
        request.backendSession = conversation.backendSession()->id;
    }

    active->turn = m_backend.start(request, m_catalog);
    if (!active->turn)
    {
        conclude(conversationId, MessageStatus::Failed, QString(), turn_error::kUnavailable);
        return;
    }
    connectTurn(conversationId, *active->turn);
}

void TurnRunner::connectTurn(const QUuid& conversationId, AgentTurn& turn)
{
    connect(&turn, &AgentTurn::sessionReady, this,
            [this, conversationId](const QString& backendSession)
            {
                if (ActiveTurn* active = findTurn(conversationId))
                {
                    active->readySession = backendSession;
                }
            });
    connect(&turn, &AgentTurn::partialText, this, [this, conversationId](const QString& text)
            { emit partialText(conversationId, text); });
    connect(&turn, &AgentTurn::stepStarted, this,
            [this, conversationId](const Step& step)
            {
                recordStep(conversationId, step);
                emit stepStarted(conversationId, step);
            });
    connect(&turn, &AgentTurn::stepFinished, this,
            [this, conversationId](const Step& step)
            {
                recordStep(conversationId, step);
                emit stepFinished(conversationId, step);
            });
    connect(&turn, &AgentTurn::finished, this, [this, conversationId](const QString& answer)
            { conclude(conversationId, MessageStatus::Complete, answer, QString()); });
    connect(&turn, &AgentTurn::failed, this,
            [this, conversationId](const QString& errorToken)
            {
                const MessageStatus status = errorToken == turn_error::kCancelled
                    ? MessageStatus::Cancelled
                    : MessageStatus::Failed;
                conclude(conversationId, status, QString(), errorToken);
            });
}

void TurnRunner::recordStep(const QUuid& conversationId, const Step& step)
{
    const ActiveTurn* active = findTurn(conversationId);
    if (!active)
    {
        return;
    }
    m_store.saveStep(active->assistantMessageId, step).warnOnError(this, "save the step");
}

void TurnRunner::conclude(const QUuid& conversationId, MessageStatus status, const QString& text,
                          const QString& error)
{
    ActiveTurn* active = findTurn(conversationId);
    if (!active)
    {
        return;
    }
    if (active->turn)
    {
        active->turn->disconnect(this);
        active->turn.release()->deleteLater();
    }

    const QStringList known = active->knownSessions();
    const std::optional<BackendSession> stored
        = status == MessageStatus::Complete ? active->sessionAfterSuccess() : std::nullopt;

    m_store.finishMessage(active->assistantMessageId, status, text, error, stored)
        .finally(this,
                 [this, conversationId, known, stored](const Result<Message>& result)
                 {
                     if (result.isFailure())
                     {
                         qWarning().noquote()
                             << "Failed to finish the chat message:" << result.error();
                     }
                     forgetAllBut(known, result.isSuccess() ? stored : std::nullopt);
                     finish(conversationId);
                 });
}

void TurnRunner::abandon(const QUuid& conversationId, const QString& reason)
{
    qWarning().noquote() << "Chat turn aborted:" << reason;
    finish(conversationId);
}

void TurnRunner::finish(const QUuid& conversationId)
{
    m_turns.erase(conversationId);
    emit turnFinished(conversationId);
    if (m_removing.count(conversationId) > 0)
    {
        removeConversation(conversationId);
    }
}

void TurnRunner::removeConversation(const QUuid& conversationId)
{
    m_store.remove(conversationId)
        .finally(this,
                 [this, conversationId](const Result<std::optional<BackendSession>>& result)
                 {
                     m_removing.erase(conversationId);
                     if (result.isFailure())
                     {
                         qWarning().noquote()
                             << "Failed to remove the conversation:" << result.error();
                         return;
                     }
                     if (result.value())
                     {
                         m_backend.forget(result.value()->id);
                     }
                     emit conversationRemoved(conversationId);
                 });
}

void TurnRunner::forgetAllBut(const QStringList& backendSessions,
                              const std::optional<BackendSession>& kept)
{
    for (const QString& session : backendSessions)
    {
        if (!kept || session != kept->id)
        {
            m_backend.forget(session);
        }
    }
}

}
