#include "chat/chatmodule.h"

#include <QMetaObject>
#include <QObject>
#include <QStringList>
#include <utility>

#include "agent/claudelauncher.h"
#include "agent/claudestdioclient.h"
#include "async/backendworker.h"
#include "chat/ui/chatsession.h"
#include "qmlutils/qmlregistrator.h"

namespace chat
{

namespace
{

    const char* const kQmlModule = "Chat.Components";

    const QString kInstructionsHead
        = QStringLiteral("You are the assistant inside %1. The conversation so far comes on stdin; "
                         "answer the user's last message with your next message only, with no "
                         "preamble.");

    const QString kInstructionsRules = QStringLiteral(
        "You never act on the app yourself. Each of your tools is one of the app's agents: give "
        "the agent whose description fits a precise, self-contained task (it does not see this "
        "conversation) and answer from its report. When no agent fits, answer from your own "
        "knowledge and say that the app was not consulted.");

}

ChatModule::Clients ChatModule::claudeClients(const QString& workingDirectory)
{
    const ClaudeLauncher launcher = claudecli::defaultLauncher(workingDirectory);
    return Clients {
        .factory = [launcher](const ClaudeSession& session) -> std::unique_ptr<::Client>
        { return std::make_unique<ClaudeStdioClient>(launcher, session); },
        .available = &ClaudeStdioClient::isAvailable,
        .forget = [launcher](const QString& id) { ClaudeStdioClient::forget(launcher, id); },
        .loginCommand = claudecli::loginCommand(launcher),
    };
}

std::unique_ptr<ChatModule> ChatModule::create(const Config& config)
{
    return create(config, claudeClients(config.workingDirectory));
}

std::unique_ptr<ChatModule> ChatModule::create(const Config& config, Clients clients)
{
    auto storage = std::make_unique<ChatStorage>(config.databasePath, config.appName);
    bool opened = false;
    QMetaObject::invokeMethod(
        config.worker, [&storage, &opened] { opened = storage->open(); },
        Qt::BlockingQueuedConnection);
    if (!opened)
    {
        return nullptr;
    }
    return std::unique_ptr<ChatModule>(
        new ChatModule(config, std::move(clients), std::move(storage)));
}

ChatModule::ChatModule(const Config& config, Clients clients, std::unique_ptr<ChatStorage> storage)
    : m_config(config)
    , m_storage(std::move(storage))
    , m_store(m_storage->conversations(), m_storage->messages(), m_storage->storage(),
              config.worker)
    , m_agent(std::move(clients.factory), clients.available, std::move(clients.forget))
    , m_runner(m_store, m_agent, m_catalog)
    , m_settings(config.settingsOrganization, config.settingsApplication)
    , m_chat(m_store, m_runner, m_catalog, m_settings,
             Chat::Config { config.settingsGroup, clients.loginCommand,
                            clients.available && clients.available() })
{
    QObject::connect(&m_chat, &Chat::modelChanged, &m_chat, [this] { refreshProfile(); });
    QObject::connect(&m_chat, &Chat::reasoningEffortChanged, &m_chat, [this] { refreshProfile(); });
    QObject::connect(&m_chat, &Chat::agentsChanged, &m_chat, [this] { refreshProfile(); });
    refreshProfile();
    m_runner.forgetSessions(m_storage->droppedSessions());
}

ChatModule::~ChatModule()
{
    m_config.worker->drain();
    QMetaObject::invokeMethod(
        m_config.worker, [this] { m_storage->close(); }, Qt::BlockingQueuedConnection);
}

bool ChatModule::addAgent(AppAgent agent)
{
    if (!m_catalog.add(std::move(agent)))
    {
        return false;
    }
    emit m_chat.agentsChanged();
    return true;
}

void ChatModule::registerQml(QmlRegistrator& registrator)
{
    registrator.registerSingletonInstance(kQmlModule, "Chat", &m_chat);
    registrator.registerType<ChatSession>(kQmlModule, "ChatSession");
}

void ChatModule::setNotes(const QString& notes)
{
    m_notes = notes;
    refreshProfile();
}

void ChatModule::refreshProfile()
{
    m_runner.setProfile(TurnProfile { mainInstructions(), m_chat.model(), m_chat.reasoningEffort(),
                                      m_chat.chosenAgentModels() });
}

Chat& ChatModule::chat() { return m_chat; }

QString ChatModule::mainInstructions() const
{
    QStringList paragraphs {
        kInstructionsHead.arg(m_config.appName),
        m_config.appPurpose,
        kInstructionsRules,
        m_notes,
    };
    paragraphs.removeAll(QString());
    return paragraphs.join(QLatin1Char('\n'));
}

}
