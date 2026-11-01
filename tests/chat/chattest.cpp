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
