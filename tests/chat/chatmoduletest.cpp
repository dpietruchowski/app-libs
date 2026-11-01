#include <gtest/gtest.h>

#include <QSettings>
#include <QStringList>
#include <QTest>
#include <memory>
#include <vector>

#include "chat/chatmodule.h"
#include "chatfixture.h"
#include "fakeclient.h"

using chat::BackendSession;
using chat::ChatModule;
using namespace chat_test;

namespace
{

constexpr int kWaitMilliseconds = 5000;

const QString kAppName = QStringLiteral("Test App");
const QString kAppPurpose = QStringLiteral("The user tests things.");
const QString kLoginCommand = QStringLiteral("login-command");

}

class ChatModuleTest : public ChatFixture
{
protected:
    void SetUp() override
    {
        ChatFixture::SetUp();
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_directory.path());
    }

    void TearDown() override
    {
        m_module.reset();
        ChatFixture::TearDown();
    }

    ChatModule::Config config()
    {
        return ChatModule::Config {
            .databasePath = databasePath(),
            .workingDirectory = m_directory.filePath(QStringLiteral("work")),
            .settingsOrganization = QStringLiteral("chatmoduletest"),
            .settingsApplication = QStringLiteral("chatmoduletest"),
            .settingsGroup = QStringLiteral("chat"),
            .worker = &m_worker,
            .appName = kAppName,
            .appPurpose = kAppPurpose,
        };
    }

    ChatModule::Clients clients()
    {
        return ChatModule::Clients {
            .factory = [this](const ClaudeSession& session) { return makeClient(session); },
            .available = [this] { return m_available; },
            .forget = [this](const QString& id) { m_forgotten.append(id); },
            .loginCommand = kLoginCommand,
        };
    }

    std::unique_ptr<::Client> makeClient(const ClaudeSession&)
    {
        auto state = std::make_shared<ClientState>();
        m_clients.push_back(state);
        return std::make_unique<ScriptedClient>(state);
    }

    void start()
    {
        m_store.reset();
        closeStorage();
        m_module = ChatModule::create(config(), clients());
        ASSERT_NE(m_module, nullptr);
    }

    QUuid startTurn(const QString& text = QStringLiteral("question"))
    {
        const size_t before = m_clients.size();
        const QUuid id = awaitOk(m_module->chat().store().createConversation(text)).id();
        EXPECT_TRUE(m_module->chat().runner().send(id, text));
        EXPECT_TRUE(QTest::qWaitFor(
            [&] { return m_clients.size() > before && !m_clients.back()->held.empty(); },
            kWaitMilliseconds));
        return id;
    }

    void finishTurn(const QUuid& id)
    {
        m_clients.back()->completeNext(contentCompletion(QStringLiteral("answer")));
        EXPECT_TRUE(QTest::qWaitFor([&] { return !m_module->chat().runner().isBusy(id); },
                                    kWaitMilliseconds));
    }

    const ClientRequest& request() const { return m_clients.back()->held.front(); }

    std::unique_ptr<ChatModule> m_module;
    std::vector<std::shared_ptr<ClientState>> m_clients;
    QStringList m_forgotten;
    bool m_available { true };
};

TEST_F(ChatModuleTest, Create_WithADatabaseThatCannotBeOpened_ReturnsNothing)
{
    ChatModule::Config unopenable = config();
    unopenable.databasePath = m_directory.filePath(QStringLiteral("missing/chat.sqlite"));

    EXPECT_EQ(ChatModule::create(unopenable, clients()), nullptr);
}

TEST_F(ChatModuleTest, Create_ForgetsTheSessionsOfTurnsTheLastRunLeftRunning)
{
    const chat::Conversation interrupted
        = conversationWithSession(QStringLiteral("A"), BackendSession { QStringLiteral("s1"), 0 });
    appendRunningAnswer(interrupted.id());
    conversationWithSession(QStringLiteral("B"), BackendSession { QStringLiteral("s2"), 0 });

    start();

    EXPECT_EQ(m_forgotten, QStringList { QStringLiteral("s1") });
}

TEST_F(ChatModuleTest, Create_WithoutInterruptedTurns_ForgetsNothing)
{
    conversationWithSession(QStringLiteral("A"), BackendSession { QStringLiteral("s1"), 0 });

    start();

    EXPECT_TRUE(m_forgotten.isEmpty());
}

TEST_F(ChatModuleTest, Chat_WhenTheClientsAreAvailable_IsAvailable)
{
    start();

    EXPECT_TRUE(m_module->chat().available());
    EXPECT_EQ(m_module->chat().loginCommand(), kLoginCommand);
}

TEST_F(ChatModuleTest, Chat_WhenTheClientsAreNotAvailable_IsUnavailable)
{
    m_available = false;

    start();

    EXPECT_FALSE(m_module->chat().available());
    EXPECT_FALSE(m_module->chat().unavailableReason().isEmpty());
}

TEST_F(ChatModuleTest, AddAgent_RejectsAKeyThatIsAlreadyTaken)
{
    start();
    const auto agent = [](const QString& key)
    {
        return chat::AppAgent { key, QStringLiteral("Courses"), QStringLiteral("Handles courses."),
                                ::Agent(QStringLiteral("haiku"), QStringLiteral("prompt")) };
    };

    EXPECT_TRUE(m_module->addAgent(agent(QStringLiteral("courses"))));
    EXPECT_FALSE(m_module->addAgent(agent(QStringLiteral("courses"))));
}

TEST_F(ChatModuleTest, Turn_StartsWithTheDefaultModelAndReasoningEffort)
{
    start();

    const QUuid id = startTurn();

    EXPECT_EQ(request().config.model, QStringLiteral("sonnet"));
    EXPECT_EQ(request().config.reasoningEffort, QStringLiteral("Minimal"));
    finishTurn(id);
}

TEST_F(ChatModuleTest, Turn_AfterTheChatSettingsChange_UsesTheNewModelAndReasoningEffort)
{
    start();
    m_module->chat().setModel(QStringLiteral("opus"));
    m_module->chat().setReasoningEffort(QStringLiteral("High"));

    const QUuid id = startTurn();

    EXPECT_EQ(request().config.model, QStringLiteral("opus"));
    EXPECT_EQ(request().config.reasoningEffort, QStringLiteral("High"));
    finishTurn(id);
}

TEST_F(ChatModuleTest, Turn_CarriesTheAppNameAndPurposeInItsInstructions)
{
    start();

    const QUuid id = startTurn();

    const QString instructions = request().messages.front().content;
    EXPECT_TRUE(instructions.contains(kAppName));
    EXPECT_TRUE(instructions.contains(kAppPurpose));
    finishTurn(id);
}

TEST_F(ChatModuleTest, SetNotes_AddsTheNotesToTheInstructionsOfTheNextTurn)
{
    start();
    const QString notes = QStringLiteral("Call the user Ada.");
    m_module->setNotes(notes);

    const QUuid id = startTurn();

    EXPECT_TRUE(request().messages.front().content.contains(notes));
    finishTurn(id);
}

TEST_F(ChatModuleTest, SetNotes_ReplacesTheNotesSetBefore)
{
    start();
    m_module->setNotes(QStringLiteral("Call the user Ada."));
    m_module->setNotes(QStringLiteral("Call the user Bob."));

    const QUuid id = startTurn();

    const QString instructions = request().messages.front().content;
    EXPECT_TRUE(instructions.contains(QStringLiteral("Call the user Bob.")));
    EXPECT_FALSE(instructions.contains(QStringLiteral("Call the user Ada.")));
    finishTurn(id);
}

TEST_F(ChatModuleTest, Turn_WithoutNotes_LeavesNoBlankLineInItsInstructions)
{
    start();
    m_module->setNotes(QStringLiteral("Call the user Ada."));
    m_module->setNotes(QString());

    const QUuid id = startTurn();

    EXPECT_FALSE(request().messages.front().content.contains(QStringLiteral("\n\n")));
    EXPECT_FALSE(request().messages.front().content.endsWith(QLatin1Char('\n')));
    finishTurn(id);
}

TEST_F(ChatModuleTest, Destroy_WhileATurnIsRunning_DestroysItsClient)
{
    start();
    startTurn();

    m_module.reset();

    EXPECT_TRUE(m_clients.back()->destroyed);
}
