#pragma once

#include <gtest/gtest.h>

#include <QDateTime>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#include "async/backendworker.h"
#include "async/mocktimeprovider.h"
#include "async/task.h"
#include "chat/application/chatstore.h"
#include "chat/infrastructure/chatstorage.h"

namespace chat_test
{

template <typename T> Result<T> await(Task<T> task) { return task.future().result(); }

template <typename T> T awaitOk(Task<T> task)
{
    const Result<T> result = await(std::move(task));
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.value();
}

inline void awaitOk(Task<void> task)
{
    const Result<void> result = await(std::move(task));
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
}

inline const QDateTime kNow = QDateTime(QDate(2026, 1, 5), QTime(12, 0), QTimeZone::utc());

inline chat::Step toolStep(chat::StepStatus status = chat::StepStatus::Running)
{
    chat::Step step;
    step.id = QUuid::createUuid();
    step.kind = chat::StepKind::Tool;
    step.agent = QStringLiteral("courses");
    step.tool = QStringLiteral("list_courses");
    step.input = QJsonObject { { QStringLiteral("limit"), 3 } };
    step.status = status;
    return step;
}

class ChatFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_directory.isValid());

        auto clock = std::make_unique<MockTimeProvider>();
        m_clock = clock.get();
        m_clock->setCurrentDateTime(kNow);
        TimeProvider::setInstance(std::move(clock));

        m_storage = std::make_unique<chat::ChatStorage>(databasePath(), QStringLiteral("chattest"));
        ASSERT_TRUE(openStorage());
        createStore();
    }

    void TearDown() override
    {
        m_store.reset();
        closeStorage();
        m_storage.reset();
        TimeProvider::setInstance(nullptr);
    }

    QString databasePath() const { return m_directory.filePath(QStringLiteral("chat.sqlite")); }

    template <typename Work> auto onWorker(Work work)
    {
        using T = std::invoke_result_t<Work>;
        return awaitOk(Task<T>::run(&m_worker, [work = std::move(work)]() mutable
                                    { return Result<T>::success(work()); }));
    }

    bool openStorage()
    {
        return onWorker([this] { return m_storage->open(); });
    }

    void closeStorage()
    {
        onWorker(
            [this]
            {
                m_storage->close();
                return true;
            });
    }

    void createStore()
    {
        m_store = std::make_unique<chat::ChatStore>(
            m_storage->conversations(), m_storage->messages(), m_storage->storage(), &m_worker);
    }

    void reopenStorage()
    {
        m_store.reset();
        closeStorage();
        EXPECT_TRUE(openStorage());
        createStore();
    }

    chat::Conversation newConversation(const QString& title = QStringLiteral("Title"))
    {
        return awaitOk(m_store->createConversation(title));
    }

    chat::Message appendMessage(const QUuid& conversationId, chat::Role role, const QString& text,
                                chat::MessageStatus status = chat::MessageStatus::Complete)
    {
        chat::Message::Data data;
        data.conversationId = conversationId;
        data.role = role;
        data.text = text;
        data.status = status;
        return awaitOk(m_store->appendMessage(data));
    }

    chat::Message appendRunningAnswer(const QUuid& conversationId,
                                      std::vector<chat::Step> steps = {})
    {
        chat::Message::Data data;
        data.conversationId = conversationId;
        data.role = chat::Role::Assistant;
        data.status = chat::MessageStatus::Running;
        data.steps = std::move(steps);
        return awaitOk(m_store->appendMessage(data));
    }

    chat::Conversation conversationWithSession(const QString& title,
                                               const chat::BackendSession& session)
    {
        const chat::Conversation conversation = newConversation(title);
        appendMessage(conversation.id(), chat::Role::User, QStringLiteral("question"));
        const chat::Message answer = appendRunningAnswer(conversation.id());
        awaitOk(m_store->finishMessage(answer.id(), chat::MessageStatus::Complete,
                                       QStringLiteral("answer"), QString(), session));
        return conversation;
    }

    std::vector<chat::Message> messagesOf(const QUuid& conversationId)
    {
        return awaitOk(m_store->messages(conversationId));
    }

    std::optional<chat::BackendSession> sessionOf(const QUuid& conversationId)
    {
        return awaitOk(m_store->conversation(conversationId)).backendSession();
    }

    void advanceClock(int seconds) { m_clock->advanceSeconds(seconds); }

    QTemporaryDir m_directory;
    BackendWorker m_worker;
    MockTimeProvider* m_clock { nullptr };
    std::unique_ptr<chat::ChatStorage> m_storage;
    std::unique_ptr<chat::ChatStore> m_store;
};

}
