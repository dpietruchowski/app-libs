#pragma once

#include <QSettings>
#include <QString>
#include <functional>
#include <memory>

#include "chat/application/agentcatalog.h"
#include "chat/application/chatagent.h"
#include "chat/application/chatstore.h"
#include "chat/application/turnrunner.h"
#include "chat/infrastructure/chatstorage.h"
#include "chat/ui/chat.h"

class BackendWorker;
class QmlRegistrator;

namespace chat
{

class ChatModule final
{
public:
    struct Config final
    {
        QString databasePath;
        QString workingDirectory;
        QString settingsOrganization;
        QString settingsApplication;
        QString settingsGroup;
        BackendWorker* worker { nullptr };
        QString appName;
        QString appPurpose;
    };

    struct Clients final
    {
        ClientFactory factory;
        std::function<bool()> available;
        std::function<void(const QString&)> forget;
        QString loginCommand;
    };

    static Clients claudeClients(const QString& workingDirectory);

    static std::unique_ptr<ChatModule> create(const Config& config);
    static std::unique_ptr<ChatModule> create(const Config& config, Clients clients);

    ~ChatModule();

    ChatModule(const ChatModule&) = delete;
    ChatModule& operator=(const ChatModule&) = delete;

    bool addAgent(AppAgent agent);
    void registerQml(QmlRegistrator& registrator);
    void setNotes(const QString& notes);

    Chat& chat();

private:
    ChatModule(const Config& config, Clients clients, std::unique_ptr<ChatStorage> storage);

    void refreshProfile();
    QString mainInstructions() const;

    Config m_config;
    QString m_notes;
    std::unique_ptr<ChatStorage> m_storage;
    ChatStore m_store;
    AgentCatalog m_catalog;
    ChatAgent m_agent;
    TurnRunner m_runner;
    QSettings m_settings;
    Chat m_chat;
};

}
