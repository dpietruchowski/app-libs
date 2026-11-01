#pragma once

#include <QHash>
#include <QObject>
#include <QSettings>
#include <QString>

#include "chat/ui/chatconversationmodel.h"

namespace chat
{

class AgentCatalog;
class ChatStore;
class TurnRunner;

class Chat : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString unavailableReason READ unavailableReason CONSTANT)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString reasoningEffort READ reasoningEffort WRITE setReasoningEffort NOTIFY
                   reasoningEffortChanged)
    Q_PROPERTY(ChatConversationModel* conversations READ conversations CONSTANT)

public:
    struct Config final
    {
        QString settingsGroup;
        QString loginCommand;
        bool available { false };
    };

    Chat(ChatStore& store, TurnRunner& runner, const AgentCatalog& catalog, QSettings& settings,
         const Config& config, QObject* parent = nullptr);

    bool available() const;
    QString unavailableReason() const;
    QString loginCommand() const;
    QHash<QString, QString> agentTitles() const;

    ChatStore& store();
    TurnRunner& runner();

    QString model() const;
    void setModel(const QString& model);

    QString reasoningEffort() const;
    void setReasoningEffort(const QString& reasoningEffort);

    ChatConversationModel* conversations();

    Q_INVOKABLE void remove(const QString& conversationId);
    Q_INVOKABLE void rename(const QString& conversationId, const QString& title);

signals:
    void modelChanged();
    void reasoningEffortChanged();

private:
    void reloadConversations();
    QString modelKey() const;
    QString reasoningEffortKey() const;

    ChatStore& m_store;
    TurnRunner& m_runner;
    const AgentCatalog& m_catalog;
    QSettings& m_settings;
    Config m_config;
    ChatConversationModel m_conversations;
};

}
