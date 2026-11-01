#include "chat/ui/chat.h"

#include <QUuid>

#include "chat/application/agentturn.h"
#include "chat/application/chatstore.h"
#include "chat/application/turnrunner.h"
#include "chat/ui/errortext.h"

namespace chat
{

namespace
{

    const QString kDefaultModel = QStringLiteral("sonnet");
    const QString kDefaultReasoningEffort = QStringLiteral("Minimal");

}

Chat::Chat(ChatStore& store, TurnRunner& runner, QSettings& settings, const Config& config,
           QObject* parent)
    : QObject(parent)
    , m_store(store)
    , m_runner(runner)
    , m_settings(settings)
    , m_config(config)
    , m_conversations(this)
{
    connect(&m_runner, &TurnRunner::turnStarted, this, &Chat::reloadConversations);
    connect(&m_runner, &TurnRunner::turnFinished, this, &Chat::reloadConversations);
    connect(&m_runner, &TurnRunner::conversationRemoved, this, &Chat::reloadConversations);
    reloadConversations();
}

bool Chat::available() const { return m_config.available; }

QString Chat::unavailableReason() const
{
    if (m_config.available)
    {
        return QString();
    }
    return errorText(MessageStatus::Failed, turn_error::kUnavailable, QString());
}

QString Chat::model() const { return m_settings.value(modelKey(), kDefaultModel).toString(); }

void Chat::setModel(const QString& model)
{
    if (this->model() == model)
    {
        return;
    }
    m_settings.setValue(modelKey(), model);
    m_settings.sync();
    emit modelChanged();
}

QString Chat::reasoningEffort() const
{
    return m_settings.value(reasoningEffortKey(), kDefaultReasoningEffort).toString();
}

void Chat::setReasoningEffort(const QString& reasoningEffort)
{
    if (this->reasoningEffort() == reasoningEffort)
    {
        return;
    }
    m_settings.setValue(reasoningEffortKey(), reasoningEffort);
    m_settings.sync();
    emit reasoningEffortChanged();
}

ChatConversationModel* Chat::conversations() { return &m_conversations; }

void Chat::remove(const QString& conversationId)
{
    const QUuid id = QUuid::fromString(conversationId);
    if (!id.isNull())
    {
        m_runner.remove(id);
    }
}

void Chat::rename(const QString& conversationId, const QString& title)
{
    const QUuid id = QUuid::fromString(conversationId);
    if (id.isNull())
    {
        return;
    }
    m_store.rename(id, title)
        .then(this, [this] { reloadConversations(); })
        .warnOnError(this, "rename the conversation");
}

void Chat::reloadConversations()
{
    m_store.conversations()
        .then(this, [this](const std::vector<Conversation>& conversations)
              { m_conversations.reset(conversations); })
        .warnOnError(this, "load the conversations");
}

QString Chat::modelKey() const { return m_config.settingsGroup + QStringLiteral("/model"); }

QString Chat::reasoningEffortKey() const
{
    return m_config.settingsGroup + QStringLiteral("/reasoningEffort");
}

}
