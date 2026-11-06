#pragma once

#include <QString>
#include <QUuid>
#include <functional>
#include <optional>
#include <vector>

#include "async/result.h"
#include "async/service.h"
#include "async/task.h"
#include "chat/domain/backendsession.h"
#include "chat/domain/chatcontext.h"
#include "chat/domain/conversation.h"
#include "chat/domain/message.h"
#include "chat/domain/step.h"

class DbStorage;

namespace chat
{

class ConversationRepository;
class MessageRepository;

class ChatStore : public Service
{
public:
    ChatStore(ConversationRepository& conversations, MessageRepository& messages,
              DbStorage& storage, QObject* worker);

    Task<std::vector<Conversation>> conversations();
    Task<Conversation> conversation(const QUuid& conversationId);
    Task<std::vector<Message>> messages(const QUuid& conversationId);

    Task<Conversation> createConversation(const QString& title,
                                          const std::optional<ChatContext>& origin = std::nullopt);
    Task<Message> appendMessage(const Message::Data& data);
    Task<void> saveStep(const QUuid& messageId, const Step& step);
    Task<Message> finishMessage(const QUuid& messageId, MessageStatus status, const QString& text,
                                const QString& error,
                                const std::optional<BackendSession>& backendSession);
    Task<Message> restartMessage(const QUuid& messageId);
    Task<void> rename(const QUuid& conversationId, const QString& title);
    Task<std::optional<BackendSession>> remove(const QUuid& conversationId);

private:
    Result<std::vector<Conversation>> conversationsCore() const;
    Result<Conversation> conversationCore(const QUuid& conversationId) const;
    Result<std::vector<Message>> messagesCore(const QUuid& conversationId) const;
    Result<Conversation> createConversationCore(const QString& title,
                                                const std::optional<ChatContext>& origin);
    Result<Message> appendMessageCore(const Message::Data& data);
    Result<void> saveStepCore(const QUuid& messageId, const Step& step);
    Result<Message> finishMessageCore(const QUuid& messageId, MessageStatus status,
                                      const QString& text, const QString& error,
                                      const std::optional<BackendSession>& backendSession);
    Result<Message> restartMessageCore(const QUuid& messageId);
    Result<void> renameCore(const QUuid& conversationId, const QString& title);
    Result<std::optional<BackendSession>> removeCore(const QUuid& conversationId);

    Result<Message> rewriteMessage(const QUuid& messageId,
                                   const std::function<void(Message::Data&, Conversation&)>& edit);

    ConversationRepository& m_conversations;
    MessageRepository& m_messages;
    DbStorage& m_storage;
};

}
