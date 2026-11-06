#include "chat/ui/chatsession.h"

#include <QJsonObject>

#include "async/task.h"
#include "chat/application/chatstore.h"
#include "chat/application/turnrunner.h"
#include "chat/domain/conversationtitle.h"

namespace chat
{

ChatSession::ChatSession(QObject* parent)
    : QObject(parent)
    , m_messages(this)
{
}

Chat* ChatSession::chat() const { return m_chat; }

void ChatSession::setChat(Chat* chat)
{
    if (m_chat == chat)
    {
        return;
    }
    detach();
    m_chat = chat;
    if (m_chat)
    {
        attach();
    }
    emit chatChanged();
}

QString ChatSession::conversationId() const
{
    return m_conversationId.isNull() ? QString() : m_conversationId.toString(QUuid::WithoutBraces);
}

void ChatSession::setConversationId(const QString& conversationId)
{
    const QUuid parsed = QUuid::fromString(conversationId);
    if (parsed == m_conversationId)
    {
        return;
    }
    m_conversationId = parsed;
    resetState();
    emit conversationIdChanged();
    loadConversation();
}

QString ChatSession::title() const { return m_title; }

ChatMessageModel* ChatSession::messages() { return &m_messages; }

bool ChatSession::busy() const { return m_busy; }

QString ChatSession::activity() const { return m_activity; }

QVariantMap ChatSession::context() const
{
    if (!m_context)
    {
        return QVariantMap();
    }
    return { { QStringLiteral("key"), m_context->key },
             { QStringLiteral("title"), m_context->title },
             { QStringLiteral("data"), m_context->data.toVariantMap() } };
}

void ChatSession::setContext(const QVariantMap& context)
{
    const QString key = context.value(QStringLiteral("key")).toString();
    std::optional<ChatContext> parsed;
    if (!key.isEmpty())
    {
        parsed = ChatContext { key, context.value(QStringLiteral("title")).toString(),
                               QJsonObject::fromVariantMap(
                                   context.value(QStringLiteral("data")).toMap()) };
    }
    if (parsed == m_context)
    {
        return;
    }
    m_context = parsed;
    emit contextChanged();
}

void ChatSession::send(const QString& text)
{
    if (!m_chat || m_busy || text.trimmed().isEmpty())
    {
        return;
    }
    clearLiveState();
    setBusy(true);

    if (!m_conversationId.isNull())
    {
        startTurn(m_conversationId, text);
        return;
    }

    m_chat->store()
        .createConversation(conversationTitle(text), m_context)
        .then(this,
              [this, text](const Conversation& conversation)
              {
                  if (m_conversationId.isNull())
                  {
                      adopt(conversation);
                  }
                  startTurn(conversation.id(), text);
              })
        .warnOnError(this, "create the conversation")
        .finally(this,
                 [this](const Result<void>& result)
                 {
                     if (result.isFailure() && m_conversationId.isNull())
                     {
                         setBusy(false);
                     }
                 });
}

void ChatSession::cancel()
{
    if (m_chat && !m_conversationId.isNull())
    {
        m_chat->runner().cancel(m_conversationId);
    }
}

void ChatSession::retry()
{
    if (!m_chat || m_busy || m_conversationId.isNull())
    {
        return;
    }
    if (m_chat->runner().retry(m_conversationId))
    {
        clearLiveState();
        setBusy(true);
    }
}

void ChatSession::attach()
{
    m_messages.setAgentTitles(m_chat->agentTitles());
    m_messages.setLoginCommand(m_chat->loginCommand());

    TurnRunner& runner = m_chat->runner();
    m_connections.append(connect(&runner, &TurnRunner::turnStarted, this,
                                 [this](const QUuid& id)
                                 {
                                     if (follows(id))
                                     {
                                         onTurnStarted();
                                     }
                                 }));
    m_connections.append(connect(&runner, &TurnRunner::partialText, this,
                                 [this](const QUuid& id, const QString& text)
                                 {
                                     if (follows(id))
                                     {
                                         onPartialText(text);
                                     }
                                 }));
    m_connections.append(connect(&runner, &TurnRunner::stepStarted, this,
                                 [this](const QUuid& id, const Step& step)
                                 {
                                     if (follows(id))
                                     {
                                         onStep(step);
                                     }
                                 }));
    m_connections.append(connect(&runner, &TurnRunner::stepFinished, this,
                                 [this](const QUuid& id, const Step& step)
                                 {
                                     if (follows(id))
                                     {
                                         onStep(step);
                                     }
                                 }));
    m_connections.append(connect(&runner, &TurnRunner::turnFinished, this,
                                 [this](const QUuid& id)
                                 {
                                     if (follows(id))
                                     {
                                         onTurnFinished();
                                     }
                                 }));
    m_connections.append(connect(&runner, &TurnRunner::conversationRemoved, this,
                                 [this](const QUuid& id)
                                 {
                                     if (follows(id))
                                     {
                                         setConversationId(QString());
                                     }
                                 }));

    loadConversation();
}

void ChatSession::detach()
{
    for (const QMetaObject::Connection& connection : m_connections)
    {
        disconnect(connection);
    }
    m_connections.clear();
    clearLiveState();
    setBusy(false);
    setActivity(QString());
}

bool ChatSession::follows(const QUuid& conversationId) const
{
    return !conversationId.isNull() && conversationId == m_conversationId;
}

void ChatSession::loadConversation()
{
    if (!m_chat || m_conversationId.isNull())
    {
        return;
    }

    const QUuid id = m_conversationId;
    m_chat->store()
        .conversation(id)
        .then(this,
              [this, id](const Conversation& conversation)
              {
                  if (id == m_conversationId)
                  {
                      setTitle(conversation.title());
                  }
              })
        .warnOnError(this, "load the conversation");
    reloadMessages();
    setBusy(m_chat->runner().isBusy(id));
}

void ChatSession::reloadMessages()
{
    if (!m_chat || m_conversationId.isNull())
    {
        return;
    }

    const QUuid id = m_conversationId;
    m_chat->store()
        .messages(id)
        .then(this,
              [this, id](const std::vector<Message>& messages)
              {
                  if (id != m_conversationId)
                  {
                      return;
                  }
                  m_messages.reset(messages);
                  if (m_liveText)
                  {
                      m_messages.setRunningText(*m_liveText);
                  }
                  for (const Step& step : m_liveSteps)
                  {
                      m_messages.putRunningStep(step);
                  }
                  refreshActivity();
                  setBusy(m_chat && m_chat->runner().isBusy(id));
              })
        .warnOnError(this, "load the messages");
}

void ChatSession::adopt(const Conversation& conversation)
{
    m_conversationId = conversation.id();
    emit conversationIdChanged();
    setTitle(conversation.title());
}

void ChatSession::startTurn(const QUuid& conversationId, const QString& text)
{
    if (m_chat && m_chat->runner().send(conversationId, text, m_context))
    {
        return;
    }
    if (conversationId == m_conversationId)
    {
        setBusy(m_chat && m_chat->runner().isBusy(conversationId));
    }
}

void ChatSession::clearLiveState()
{
    m_liveText.reset();
    m_liveSteps.clear();
}

void ChatSession::resetState()
{
    clearLiveState();
    m_messages.reset({});
    setTitle(QString());
    setBusy(false);
    setActivity(QString());
}

void ChatSession::refreshActivity() { setActivity(m_messages.activity()); }

void ChatSession::setTitle(const QString& title)
{
    if (m_title == title)
    {
        return;
    }
    m_title = title;
    emit titleChanged();
}

void ChatSession::setBusy(bool busy)
{
    if (m_busy == busy)
    {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void ChatSession::setActivity(const QString& activity)
{
    if (m_activity == activity)
    {
        return;
    }
    m_activity = activity;
    emit activityChanged();
}

void ChatSession::onPartialText(const QString& text)
{
    m_liveText = text;
    m_messages.setRunningText(text);
}

void ChatSession::onStep(const Step& step)
{
    upsertStep(m_liveSteps, step);
    m_messages.putRunningStep(step);
    refreshActivity();
}

void ChatSession::onTurnStarted()
{
    clearLiveState();
    setBusy(true);
    reloadMessages();
}

void ChatSession::onTurnFinished()
{
    clearLiveState();
    setActivity(QString());
    setBusy(false);
    reloadMessages();
}

}
