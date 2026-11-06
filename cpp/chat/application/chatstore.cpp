#include "chat/application/chatstore.h"

#include "async/timeprovider.h"
#include "chat/domain/conversationrepository.h"
#include "chat/domain/messagerepository.h"
#include "dbtoolkit/dbstorage.h"
#include "dbtoolkit/transaction.h"

namespace chat
{

namespace
{
    const QString kConversationNotFound = QStringLiteral("Conversation not found");
    const QString kMessageNotFound = QStringLiteral("Message not found");
    const QString kTransactionFailed = QStringLiteral("Cannot start a database transaction");
    const QString kSaveConversationFailed = QStringLiteral("Cannot save the conversation");
    const QString kSaveMessageFailed = QStringLiteral("Cannot save the message");
    const QString kSaveStepFailed = QStringLiteral("Cannot save the step");
    const QString kRemoveStepsFailed = QStringLiteral("Cannot remove the steps of the message");
    const QString kRemoveConversationFailed = QStringLiteral("Cannot remove the conversation");
    const QString kCommitFailed = QStringLiteral("Cannot commit the changes");
    const QString kBlankTitle = QStringLiteral("The title is empty");

    QDateTime currentTime() { return TimeProvider::instance().currentDateTime(); }
}

ChatStore::ChatStore(ConversationRepository& conversations, MessageRepository& messages,
                     DbStorage& storage, QObject* worker)
    : Service(worker)
    , m_conversations(conversations)
    , m_messages(messages)
    , m_storage(storage)
{
}

Task<std::vector<Conversation>> ChatStore::conversations()
{
    return invoke([this] { return conversationsCore(); });
}

Task<Conversation> ChatStore::conversation(const QUuid& conversationId)
{
    return invoke([this, conversationId] { return conversationCore(conversationId); });
}

Task<std::vector<Message>> ChatStore::messages(const QUuid& conversationId)
{
    return invoke([this, conversationId] { return messagesCore(conversationId); });
}

Task<Conversation> ChatStore::createConversation(const QString& title,
                                                 const std::optional<ChatContext>& origin)
{
    return invoke([this, title, origin] { return createConversationCore(title, origin); });
}

Task<Message> ChatStore::appendMessage(const Message::Data& data)
{
    return invoke([this, data] { return appendMessageCore(data); });
}

Task<void> ChatStore::saveStep(const QUuid& messageId, const Step& step)
{
    return invoke([this, messageId, step] { return saveStepCore(messageId, step); });
}

Task<Message> ChatStore::finishMessage(const QUuid& messageId, MessageStatus status,
                                       const QString& text, const QString& error,
                                       const std::optional<BackendSession>& backendSession)
{
    return invoke([this, messageId, status, text, error, backendSession]
                  { return finishMessageCore(messageId, status, text, error, backendSession); });
}

Task<Message> ChatStore::restartMessage(const QUuid& messageId)
{
    return invoke([this, messageId] { return restartMessageCore(messageId); });
}

Task<void> ChatStore::rename(const QUuid& conversationId, const QString& title)
{
    return invoke([this, conversationId, title] { return renameCore(conversationId, title); });
}

Task<std::optional<BackendSession>> ChatStore::remove(const QUuid& conversationId)
{
    return invoke([this, conversationId] { return removeCore(conversationId); });
}

Result<std::vector<Conversation>> ChatStore::conversationsCore() const
{
    return Result<std::vector<Conversation>>::success(m_conversations.findAll());
}

Result<Conversation> ChatStore::conversationCore(const QUuid& conversationId) const
{
    const std::optional<Conversation> found = m_conversations.findById(conversationId);
    if (!found)
    {
        return Result<Conversation>::failure(kConversationNotFound);
    }
    return Result<Conversation>::success(*found);
}

Result<std::vector<Message>> ChatStore::messagesCore(const QUuid& conversationId) const
{
    return Result<std::vector<Message>>::success(m_messages.findByConversation(conversationId));
}

Result<Conversation> ChatStore::createConversationCore(const QString& title,
                                                       const std::optional<ChatContext>& origin)
{
    const Conversation conversation
        = Conversation::create(Conversation::Data { title, origin }, currentTime());
    if (!m_conversations.save(conversation))
    {
        return Result<Conversation>::failure(kSaveConversationFailed);
    }
    return Result<Conversation>::success(conversation);
}

Result<Message> ChatStore::appendMessageCore(const Message::Data& data)
{
    Transaction transaction(m_storage);
    if (!transaction.isActive())
    {
        return Result<Message>::failure(kTransactionFailed);
    }

    std::optional<Conversation> conversation = m_conversations.findById(data.conversationId);
    if (!conversation)
    {
        return Result<Message>::failure(kConversationNotFound);
    }

    const QDateTime now = currentTime();
    Message::Data stored = data;
    stored.position = m_messages.nextPosition(data.conversationId);
    const Message message = Message::create(stored, now);

    if (!m_messages.saveMessage(message))
    {
        return Result<Message>::failure(kSaveMessageFailed);
    }
    for (const Step& step : message.steps())
    {
        if (!m_messages.saveStep(message.id(), step, now))
        {
            return Result<Message>::failure(kSaveStepFailed);
        }
    }

    conversation->touch(now);
    if (!m_conversations.save(*conversation))
    {
        return Result<Message>::failure(kSaveConversationFailed);
    }
    if (!transaction.commit())
    {
        return Result<Message>::failure(kCommitFailed);
    }
    return Result<Message>::success(message);
}

Result<void> ChatStore::saveStepCore(const QUuid& messageId, const Step& step)
{
    if (!m_messages.saveStep(messageId, step, currentTime()))
    {
        return Result<void>::failure(kSaveStepFailed);
    }
    return Result<void>::success();
}

Result<Message> ChatStore::finishMessageCore(const QUuid& messageId, MessageStatus status,
                                             const QString& text, const QString& error,
                                             const std::optional<BackendSession>& backendSession)
{
    return rewriteMessage(
        messageId,
        [status, &text, &error, &backendSession](Message::Data& data, Conversation& conversation)
        {
            data.status = status;
            data.text = text;
            data.error = error;
            conversation.setBackendSession(backendSession);
        });
}

Result<Message> ChatStore::restartMessageCore(const QUuid& messageId)
{
    Transaction transaction(m_storage);
    if (!transaction.isActive())
    {
        return Result<Message>::failure(kTransactionFailed);
    }
    if (!m_messages.removeSteps(messageId))
    {
        return Result<Message>::failure(kRemoveStepsFailed);
    }

    const Result<Message> restarted = rewriteMessage(messageId,
                                                     [](Message::Data& data, Conversation&)
                                                     {
                                                         data.status = MessageStatus::Running;
                                                         data.text.clear();
                                                         data.error.clear();
                                                         data.steps.clear();
                                                     });
    if (restarted.isFailure())
    {
        return restarted;
    }
    if (!transaction.commit())
    {
        return Result<Message>::failure(kCommitFailed);
    }
    return restarted;
}

Result<void> ChatStore::renameCore(const QUuid& conversationId, const QString& title)
{
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty())
    {
        return Result<void>::failure(kBlankTitle);
    }

    std::optional<Conversation> conversation = m_conversations.findById(conversationId);
    if (!conversation)
    {
        return Result<void>::failure(kConversationNotFound);
    }

    conversation->rename(trimmed);
    if (!m_conversations.save(*conversation))
    {
        return Result<void>::failure(kSaveConversationFailed);
    }
    return Result<void>::success();
}

Result<std::optional<BackendSession>> ChatStore::removeCore(const QUuid& conversationId)
{
    const std::optional<Conversation> conversation = m_conversations.findById(conversationId);
    if (!conversation)
    {
        return Result<std::optional<BackendSession>>::success(std::nullopt);
    }
    if (!m_conversations.remove(conversationId))
    {
        return Result<std::optional<BackendSession>>::failure(kRemoveConversationFailed);
    }
    return Result<std::optional<BackendSession>>::success(conversation->backendSession());
}

Result<Message>
ChatStore::rewriteMessage(const QUuid& messageId,
                          const std::function<void(Message::Data&, Conversation&)>& edit)
{
    Transaction transaction(m_storage);
    if (!transaction.isActive())
    {
        return Result<Message>::failure(kTransactionFailed);
    }

    const std::optional<Message> existing = m_messages.findById(messageId);
    if (!existing)
    {
        return Result<Message>::failure(kMessageNotFound);
    }
    std::optional<Conversation> conversation = m_conversations.findById(existing->conversationId());
    if (!conversation)
    {
        return Result<Message>::failure(kConversationNotFound);
    }

    Message::Data data = existing->data();
    edit(data, *conversation);
    const Message rewritten(existing->id(), data, existing->createdAt());
    if (!m_messages.saveMessage(rewritten))
    {
        return Result<Message>::failure(kSaveMessageFailed);
    }

    conversation->touch(currentTime());
    if (!m_conversations.save(*conversation))
    {
        return Result<Message>::failure(kSaveConversationFailed);
    }
    if (!transaction.commit())
    {
        return Result<Message>::failure(kCommitFailed);
    }
    return Result<Message>::success(rewritten);
}

}
