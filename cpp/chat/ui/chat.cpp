#include "chat/ui/chat.h"

#include <QUuid>
#include <QVariantMap>

#include "chat/application/agentcatalog.h"
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

Chat::Chat(ChatStore& store, TurnRunner& runner, const AgentCatalog& catalog, QSettings& settings,
           const Config& config, QObject* parent)
    : QObject(parent)
    , m_store(store)
    , m_runner(runner)
    , m_catalog(catalog)
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

QString Chat::loginCommand() const { return m_config.loginCommand; }

QHash<QString, QString> Chat::agentTitles() const
{
    QHash<QString, QString> titles;
    for (const AppAgent& agent : m_catalog.agents())
    {
        titles.insert(agent.key, agent.title);
    }
    return titles;
}

ChatStore& Chat::store() { return m_store; }

TurnRunner& Chat::runner() { return m_runner; }

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
    emit agentsChanged();
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

QVariantList Chat::agents() const
{
    QVariantList agents;
    for (const AppAgent& agent : m_catalog.agents())
    {
        const QString chosen = chosenAgentModel(agent.key);
        agents.append(QVariantMap {
            { QStringLiteral("key"), agent.key },
            { QStringLiteral("title"), agent.title },
            { QStringLiteral("chosen"), chosen },
            { QStringLiteral("model"), agent.modelFor(chosen, model()) },
        });
    }
    return agents;
}

QHash<QString, QString> Chat::chosenAgentModels() const
{
    QHash<QString, QString> chosen;
    for (const AppAgent& agent : m_catalog.agents())
    {
        const QString model = chosenAgentModel(agent.key);
        if (!model.isEmpty())
        {
            chosen.insert(agent.key, model);
        }
    }
    return chosen;
}

void Chat::setAgentModel(const QString& key, const QString& model)
{
    if (!m_catalog.find(key) || chosenAgentModel(key) == model)
    {
        return;
    }
    if (model.isEmpty())
    {
        m_settings.remove(agentModelKey(key));
    }
    else
    {
        m_settings.setValue(agentModelKey(key), model);
    }
    m_settings.sync();
    emit agentsChanged();
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
              { m_conversations.update(conversations); })
        .warnOnError(this, "load the conversations");
}

QString Chat::modelKey() const { return m_config.settingsGroup + QStringLiteral("/model"); }

QString Chat::reasoningEffortKey() const
{
    return m_config.settingsGroup + QStringLiteral("/reasoningEffort");
}

QString Chat::agentModelKey(const QString& agentKey) const
{
    return m_config.settingsGroup + QStringLiteral("/agentModels/") + agentKey;
}

QString Chat::chosenAgentModel(const QString& agentKey) const
{
    return m_settings.value(agentModelKey(agentKey)).toString();
}

}
