#include <gtest/gtest.h>

#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <memory>

#include "chat/application/agentcatalog.h"
#include "chat/application/agentturn.h"
#include "chat/application/turnrunner.h"
#include "chat/ui/chat.h"
#include "chat/ui/errortext.h"
#include "chatfixture.h"
#include "fakeagentbackend.h"

using namespace chat;
using namespace chat_test;

namespace
{

constexpr int kWaitMilliseconds = 5000;

QString idText(const Conversation& conversation)
{
    return conversation.id().toString(QUuid::WithoutBraces);
}

}

class ChatTest : public ChatFixture
{
protected:
    void SetUp() override
    {
        ChatFixture::SetUp();
        m_runner = std::make_unique<TurnRunner>(*m_store, m_backend, m_catalog);
    }

    void TearDown() override
    {
        m_worker.drain();
        m_chat.reset();
        m_settings.reset();
        m_runner.reset();
        ChatFixture::TearDown();
    }

    void createChat(bool available = true, const QString& settingsGroup = QStringLiteral("chat"),
                    const QString& loginCommand = QString())
    {
        m_worker.drain();
        m_chat.reset();
        m_settings = std::make_unique<QSettings>(m_directory.filePath(QStringLiteral("chat.ini")),
                                                 QSettings::IniFormat);
        m_chat = std::make_unique<Chat>(*m_store, *m_runner, m_catalog, *m_settings,
                                        Chat::Config { settingsGroup, loginCommand, available });
    }

    template <typename Predicate> bool waitUntil(Predicate predicate)
    {
        return QTest::qWaitFor(predicate, kWaitMilliseconds);
    }

    void addAgent(const QString& key, const QString& ownModel = QString())
    {
        AppAgent agent { key, key.toUpper(), QStringLiteral("Does the work."),
                         ::Agent(QStringLiteral("model"), QStringLiteral("prompt")) };
        agent.config.model = ownModel;
        ASSERT_TRUE(m_catalog.add(std::move(agent)));
    }

    QVariantMap agentAt(int index) const { return m_chat->agents().at(index).toMap(); }

    QString titleAt(int row) const
    {
        const ChatConversationModel* model = m_chat->conversations();
        return model->data(model->index(row), ChatConversationModel::TitleRole).toString();
    }

    QString idAt(int row) const
    {
        const ChatConversationModel* model = m_chat->conversations();
        return model->data(model->index(row), ChatConversationModel::ConversationIdRole).toString();
    }

    int rows() const { return m_chat->conversations()->rowCount(); }

    AgentCatalog m_catalog;
    FakeBackend m_backend;
    std::unique_ptr<TurnRunner> m_runner;
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<Chat> m_chat;
};

TEST_F(ChatTest, Defaults_AreTheSonnetModelAndMinimalEffort)
{
    createChat();

    EXPECT_EQ(m_chat->model(), QStringLiteral("sonnet"));
    EXPECT_EQ(m_chat->reasoningEffort(), QStringLiteral("Minimal"));
}

TEST_F(ChatTest, SetModel_StoresTheModelAndNotifies)
{
    createChat();
    QSignalSpy changed(m_chat.get(), &Chat::modelChanged);

    m_chat->setModel(QStringLiteral("opus"));

    EXPECT_EQ(m_chat->model(), QStringLiteral("opus"));
    EXPECT_EQ(changed.count(), 1);
}

TEST_F(ChatTest, SetModel_ToTheCurrentModel_NotifiesNothing)
{
    createChat();
    m_chat->setModel(QStringLiteral("opus"));
    QSignalSpy changed(m_chat.get(), &Chat::modelChanged);

    m_chat->setModel(QStringLiteral("opus"));

    EXPECT_EQ(changed.count(), 0);
}

TEST_F(ChatTest, SetReasoningEffort_StoresTheEffortAndNotifies)
{
    createChat();
    QSignalSpy changed(m_chat.get(), &Chat::reasoningEffortChanged);

    m_chat->setReasoningEffort(QStringLiteral("High"));

    EXPECT_EQ(m_chat->reasoningEffort(), QStringLiteral("High"));
    EXPECT_EQ(changed.count(), 1);
}

TEST_F(ChatTest, SetReasoningEffort_ToTheCurrentEffort_NotifiesNothing)
{
    createChat();
    m_chat->setReasoningEffort(QStringLiteral("High"));
    QSignalSpy changed(m_chat.get(), &Chat::reasoningEffortChanged);

    m_chat->setReasoningEffort(QStringLiteral("High"));

    EXPECT_EQ(changed.count(), 0);
}

TEST_F(ChatTest, TheChoices_SurviveARecreatedChat)
{
    createChat();
    m_chat->setModel(QStringLiteral("opus"));
    m_chat->setReasoningEffort(QStringLiteral("High"));

    createChat();

    EXPECT_EQ(m_chat->model(), QStringLiteral("opus"));
    EXPECT_EQ(m_chat->reasoningEffort(), QStringLiteral("High"));
}

TEST_F(ChatTest, TheChoices_LiveUnderTheConfiguredGroup)
{
    createChat(true, QStringLiteral("assistant"));

    m_chat->setModel(QStringLiteral("opus"));
    m_chat->setReasoningEffort(QStringLiteral("High"));

    QSettings reread(m_directory.filePath(QStringLiteral("chat.ini")), QSettings::IniFormat);
    EXPECT_EQ(reread.value(QStringLiteral("assistant/model")).toString(), QStringLiteral("opus"));
    EXPECT_EQ(reread.value(QStringLiteral("assistant/reasoningEffort")).toString(),
              QStringLiteral("High"));
}

TEST_F(ChatTest, Agents_WithoutAppAgents_AreEmpty)
{
    createChat();

    EXPECT_TRUE(m_chat->agents().isEmpty());
    EXPECT_TRUE(m_chat->chosenAgentModels().isEmpty());
}

TEST_F(ChatTest, Agents_ListEveryAppAgentWithItsKeyTitleAndTheModelItRunsOn)
{
    addAgent(QStringLiteral("courses"));
    addAgent(QStringLiteral("facts"), QStringLiteral("opus"));
    createChat();

    ASSERT_EQ(m_chat->agents().size(), 2);
    EXPECT_EQ(agentAt(0).value(QStringLiteral("key")).toString(), QStringLiteral("courses"));
    EXPECT_EQ(agentAt(0).value(QStringLiteral("title")).toString(), QStringLiteral("COURSES"));
    EXPECT_EQ(agentAt(0).value(QStringLiteral("model")).toString(), QStringLiteral("sonnet"));
    EXPECT_EQ(agentAt(0).value(QStringLiteral("chosen")).toString(), QString());
    EXPECT_EQ(agentAt(1).value(QStringLiteral("key")).toString(), QStringLiteral("facts"));
    EXPECT_EQ(agentAt(1).value(QStringLiteral("model")).toString(), QStringLiteral("opus"));
}

TEST_F(ChatTest, SetAgentModel_StoresTheChoiceAndNotifies)
{
    addAgent(QStringLiteral("courses"), QStringLiteral("opus"));
    createChat();
    QSignalSpy changed(m_chat.get(), &Chat::agentsChanged);

    m_chat->setAgentModel(QStringLiteral("courses"), QStringLiteral("haiku"));

    EXPECT_EQ(agentAt(0).value(QStringLiteral("model")).toString(), QStringLiteral("haiku"));
    EXPECT_EQ(agentAt(0).value(QStringLiteral("chosen")).toString(), QStringLiteral("haiku"));
    EXPECT_EQ(m_chat->chosenAgentModels(),
              (QHash<QString, QString> { { QStringLiteral("courses"), QStringLiteral("haiku") } }));
    EXPECT_EQ(changed.count(), 1);
}

TEST_F(ChatTest, SetAgentModel_ToTheCurrentChoice_NotifiesNothing)
{
    addAgent(QStringLiteral("courses"));
    createChat();
    m_chat->setAgentModel(QStringLiteral("courses"), QStringLiteral("haiku"));
    QSignalSpy changed(m_chat.get(), &Chat::agentsChanged);

    m_chat->setAgentModel(QStringLiteral("courses"), QStringLiteral("haiku"));

    EXPECT_EQ(changed.count(), 0);
}

TEST_F(ChatTest, SetAgentModel_Empty_ReturnsToTheDefaultOfTheAgent)
{
    addAgent(QStringLiteral("courses"), QStringLiteral("opus"));
    createChat();
    m_chat->setAgentModel(QStringLiteral("courses"), QStringLiteral("haiku"));
    QSignalSpy changed(m_chat.get(), &Chat::agentsChanged);

    m_chat->setAgentModel(QStringLiteral("courses"), QString());

    EXPECT_EQ(agentAt(0).value(QStringLiteral("model")).toString(), QStringLiteral("opus"));
    EXPECT_TRUE(m_chat->chosenAgentModels().isEmpty());
    EXPECT_EQ(changed.count(), 1);
}

TEST_F(ChatTest, SetAgentModel_ForAnUnknownKey_ChangesNothing)
{
    addAgent(QStringLiteral("courses"));
    createChat();
    QSignalSpy changed(m_chat.get(), &Chat::agentsChanged);

    m_chat->setAgentModel(QStringLiteral("facts"), QStringLiteral("haiku"));

    EXPECT_EQ(changed.count(), 0);
    EXPECT_TRUE(m_chat->chosenAgentModels().isEmpty());
    QSettings reread(m_directory.filePath(QStringLiteral("chat.ini")), QSettings::IniFormat);
    EXPECT_FALSE(reread.contains(QStringLiteral("chat/agentModels/facts")));
}

TEST_F(ChatTest, AnAgentWithoutAChoiceOrAnOwnModel_FollowsTheModelOfTheChat)
{
    addAgent(QStringLiteral("courses"));
    createChat();
    QSignalSpy changed(m_chat.get(), &Chat::agentsChanged);

    m_chat->setModel(QStringLiteral("opus"));

    EXPECT_EQ(agentAt(0).value(QStringLiteral("model")).toString(), QStringLiteral("opus"));
    EXPECT_EQ(changed.count(), 1);
}

TEST_F(ChatTest, AChosenAgentModel_DoesNotFollowTheModelOfTheChat)
{
    addAgent(QStringLiteral("courses"));
    createChat();
    m_chat->setAgentModel(QStringLiteral("courses"), QStringLiteral("haiku"));

    m_chat->setModel(QStringLiteral("opus"));

    EXPECT_EQ(agentAt(0).value(QStringLiteral("model")).toString(), QStringLiteral("haiku"));
}

TEST_F(ChatTest, TheAgentChoices_SurviveARecreatedChat)
{
    addAgent(QStringLiteral("courses"));
    addAgent(QStringLiteral("facts"));
    createChat();
    m_chat->setAgentModel(QStringLiteral("facts"), QStringLiteral("haiku"));

    createChat();

    EXPECT_EQ(m_chat->chosenAgentModels(),
              (QHash<QString, QString> { { QStringLiteral("facts"), QStringLiteral("haiku") } }));
    EXPECT_EQ(agentAt(1).value(QStringLiteral("model")).toString(), QStringLiteral("haiku"));
}

TEST_F(ChatTest, TheAgentChoices_LiveNextToTheModelUnderTheConfiguredGroup)
{
    addAgent(QStringLiteral("courses"));
    createChat(true, QStringLiteral("assistant"));

    m_chat->setAgentModel(QStringLiteral("courses"), QStringLiteral("haiku"));

    QSettings reread(m_directory.filePath(QStringLiteral("chat.ini")), QSettings::IniFormat);
    EXPECT_EQ(reread.value(QStringLiteral("assistant/agentModels/courses")).toString(),
              QStringLiteral("haiku"));
}

TEST_F(ChatTest, WhenAvailable_ThereIsNoReason)
{
    createChat(true);

    EXPECT_TRUE(m_chat->available());
    EXPECT_TRUE(m_chat->unavailableReason().isEmpty());
}

TEST_F(ChatTest, WhenUnavailable_TheReasonIsTheSentenceOfTheUnavailableError)
{
    createChat(false);

    EXPECT_FALSE(m_chat->available());
    EXPECT_FALSE(m_chat->unavailableReason().isEmpty());
    EXPECT_EQ(m_chat->unavailableReason(),
              errorText(MessageStatus::Failed, turn_error::kUnavailable, QString()));
}

TEST_F(ChatTest, TheLoginCommand_IsTheConfiguredOne)
{
    createChat(true, QStringLiteral("chat"), QStringLiteral("claude auth login"));

    EXPECT_EQ(m_chat->loginCommand(), QStringLiteral("claude auth login"));
}

TEST_F(ChatTest, TheAgentTitles_AreTakenFromTheCatalogWhenAsked)
{
    createChat();
    EXPECT_TRUE(m_chat->agentTitles().isEmpty());

    m_catalog.add(AppAgent { QStringLiteral("courses"), QStringLiteral("Courses"),
                             QStringLiteral("Answers questions about courses."),
                             ::Agent(QStringLiteral("model"), QStringLiteral("prompt")) });

    EXPECT_EQ(
        m_chat->agentTitles(),
        (QHash<QString, QString> { { QStringLiteral("courses"), QStringLiteral("Courses") } }));
}

TEST_F(ChatTest, TheStoreAndTheRunner_AreTheOnesItWasBuiltFrom)
{
    createChat();

    EXPECT_EQ(&m_chat->store(), m_store.get());
    EXPECT_EQ(&m_chat->runner(), m_runner.get());
}

TEST_F(ChatTest, TheConversationList_IsLoadedAtConstructionNewestFirst)
{
    const Conversation first = newConversation(QStringLiteral("First"));
    const Conversation second = newConversation(QStringLiteral("Second"));

    createChat();

    ASSERT_TRUE(waitUntil([&] { return rows() == 2; }));
    EXPECT_EQ(idAt(0), idText(second));
    EXPECT_EQ(idAt(1), idText(first));
    EXPECT_EQ(titleAt(0), QStringLiteral("Second"));
}

TEST_F(ChatTest, Remove_DeletesTheConversationFromTheStoreAndTheList)
{
    const Conversation kept = newConversation(QStringLiteral("Kept"));
    const Conversation dropped = newConversation(QStringLiteral("Dropped"));
    createChat();
    ASSERT_TRUE(waitUntil([&] { return rows() == 2; }));

    m_chat->remove(idText(dropped));

    ASSERT_TRUE(waitUntil([&] { return rows() == 1; }));
    EXPECT_EQ(idAt(0), idText(kept));
    EXPECT_EQ(awaitOk(m_store->conversations()).size(), 1u);
}

TEST_F(ChatTest, Remove_WithATextThatIsNotAnId_ChangesNothing)
{
    newConversation(QStringLiteral("Kept"));
    createChat();
    ASSERT_TRUE(waitUntil([&] { return rows() == 1; }));

    m_chat->remove(QStringLiteral("not an id"));
    m_chat->remove(QString());

    EXPECT_EQ(awaitOk(m_store->conversations()).size(), 1u);
    EXPECT_EQ(rows(), 1);
}

TEST_F(ChatTest, Rename_ChangesTheTitleInTheStoreAndTheList)
{
    const Conversation conversation = newConversation(QStringLiteral("Old"));
    createChat();
    ASSERT_TRUE(waitUntil([&] { return rows() == 1; }));

    m_chat->rename(idText(conversation), QStringLiteral("Renamed"));

    ASSERT_TRUE(waitUntil([&] { return titleAt(0) == QStringLiteral("Renamed"); }));
    EXPECT_EQ(awaitOk(m_store->conversation(conversation.id())).title(), QStringLiteral("Renamed"));
}

TEST_F(ChatTest, Rename_ToABlankTitle_KeepsTheTitle)
{
    const Conversation conversation = newConversation(QStringLiteral("Old"));
    createChat();
    ASSERT_TRUE(waitUntil([&] { return rows() == 1; }));

    m_chat->rename(idText(conversation), QStringLiteral("   "));

    EXPECT_EQ(awaitOk(m_store->conversation(conversation.id())).title(), QStringLiteral("Old"));
    EXPECT_EQ(titleAt(0), QStringLiteral("Old"));
}

TEST_F(ChatTest, Rename_WithATextThatIsNotAnId_ChangesNothing)
{
    const Conversation conversation = newConversation(QStringLiteral("Old"));
    createChat();
    ASSERT_TRUE(waitUntil([&] { return rows() == 1; }));

    m_chat->rename(QStringLiteral("not an id"), QStringLiteral("Renamed"));

    EXPECT_EQ(awaitOk(m_store->conversation(conversation.id())).title(), QStringLiteral("Old"));
    EXPECT_EQ(titleAt(0), QStringLiteral("Old"));
}

TEST_F(ChatTest, ATurn_MovesItsConversationToTheTopOfTheList)
{
    const Conversation older = newConversation(QStringLiteral("Older"));
    const Conversation newer = newConversation(QStringLiteral("Newer"));
    createChat();
    ASSERT_TRUE(waitUntil([&] { return rows() == 2; }));
    ASSERT_EQ(idAt(0), idText(newer));
    advanceClock(60);

    ASSERT_TRUE(m_runner->send(older.id(), QStringLiteral("question")));

    ASSERT_TRUE(waitUntil([&] { return idAt(0) == idText(older); }));
    ASSERT_TRUE(waitUntil([&] { return !m_backend.latest.isNull(); }));
    m_backend.latest->complete(QStringLiteral("answer"));
    ASSERT_TRUE(waitUntil([&] { return !m_runner->isBusy(older.id()); }));
}

TEST_F(ChatTest, ATurn_UpdatesTheRowsOfTheListWithoutResettingIt)
{
    const Conversation older = newConversation(QStringLiteral("Older"));
    const Conversation newer = newConversation(QStringLiteral("Newer"));
    createChat();
    ASSERT_TRUE(waitUntil([&] { return rows() == 2; }));
    advanceClock(60);
    QSignalSpy resets(m_chat->conversations(), &QAbstractItemModel::modelReset);
    QSignalSpy moves(m_chat->conversations(), &QAbstractItemModel::rowsMoved);
    QSignalSpy changes(m_chat->conversations(), &QAbstractItemModel::dataChanged);

    ASSERT_TRUE(m_runner->send(older.id(), QStringLiteral("question")));

    ASSERT_TRUE(waitUntil([&] { return moves.count() == 1; }));
    EXPECT_EQ(idAt(0), idText(older));
    EXPECT_EQ(idAt(1), idText(newer));
    ASSERT_TRUE(waitUntil([&] { return !m_backend.latest.isNull(); }));
    const int changesAtStart = changes.count();
    advanceClock(60);
    m_backend.latest->complete(QStringLiteral("answer"));
    ASSERT_TRUE(waitUntil([&] { return !m_runner->isBusy(older.id()); }));
    ASSERT_TRUE(waitUntil([&] { return changes.count() > changesAtStart; }));
    EXPECT_EQ(changes.last().at(0).value<QModelIndex>().row(), 0);
    EXPECT_EQ(changes.last().at(2).value<QList<int>>(),
              QList<int> { ChatConversationModel::UpdatedAtRole });
    EXPECT_EQ(moves.count(), 1);
    EXPECT_EQ(resets.count(), 0);
}

TEST_F(ChatTest, TheTurnOfANewConversation_InsertsItsRowWithoutResettingTheList)
{
    const Conversation existing = newConversation(QStringLiteral("Existing"));
    createChat();
    ASSERT_TRUE(waitUntil([&] { return rows() == 1; }));
    advanceClock(60);
    const Conversation fresh = newConversation(QStringLiteral("Fresh"));
    QSignalSpy resets(m_chat->conversations(), &QAbstractItemModel::modelReset);
    QSignalSpy inserts(m_chat->conversations(), &QAbstractItemModel::rowsInserted);

    ASSERT_TRUE(m_runner->send(fresh.id(), QStringLiteral("question")));

    ASSERT_TRUE(waitUntil([&] { return inserts.count() == 1; }));
    EXPECT_EQ(inserts.first().at(1).toInt(), 0);
    EXPECT_EQ(inserts.first().at(2).toInt(), 0);
    EXPECT_EQ(idAt(0), idText(fresh));
    EXPECT_EQ(idAt(1), idText(existing));
    EXPECT_EQ(resets.count(), 0);
    ASSERT_TRUE(waitUntil([&] { return !m_backend.latest.isNull(); }));
    m_backend.latest->complete(QStringLiteral("answer"));
    ASSERT_TRUE(waitUntil([&] { return !m_runner->isBusy(fresh.id()); }));
}

TEST_F(ChatTest, Remove_RemovesOnlyTheRowOfTheConversationWithoutResettingTheList)
{
    const Conversation kept = newConversation(QStringLiteral("Kept"));
    const Conversation dropped = newConversation(QStringLiteral("Dropped"));
    createChat();
    ASSERT_TRUE(waitUntil([&] { return rows() == 2; }));
    QSignalSpy resets(m_chat->conversations(), &QAbstractItemModel::modelReset);
    QSignalSpy removals(m_chat->conversations(), &QAbstractItemModel::rowsRemoved);

    m_chat->remove(idText(dropped));

    ASSERT_TRUE(waitUntil([&] { return removals.count() == 1; }));
    EXPECT_EQ(removals.first().at(1).toInt(), 0);
    EXPECT_EQ(removals.first().at(2).toInt(), 0);
    EXPECT_EQ(idAt(0), idText(kept));
    EXPECT_EQ(resets.count(), 0);
}

TEST_F(ChatTest, Rename_ChangesOnlyTheTitleOfItsRowWithoutResettingTheList)
{
    const Conversation conversation = newConversation(QStringLiteral("Old"));
    createChat();
    ASSERT_TRUE(waitUntil([&] { return rows() == 1; }));
    QSignalSpy resets(m_chat->conversations(), &QAbstractItemModel::modelReset);
    QSignalSpy changes(m_chat->conversations(), &QAbstractItemModel::dataChanged);

    m_chat->rename(idText(conversation), QStringLiteral("Renamed"));

    ASSERT_TRUE(waitUntil([&] { return changes.count() == 1; }));
    EXPECT_EQ(changes.first().at(0).value<QModelIndex>().row(), 0);
    EXPECT_TRUE(
        changes.first().at(2).value<QList<int>>().contains(ChatConversationModel::TitleRole));
    EXPECT_EQ(resets.count(), 0);
}
