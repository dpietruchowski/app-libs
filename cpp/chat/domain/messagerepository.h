#pragma once

#include <QDateTime>
#include <QUuid>
#include <optional>
#include <vector>

#include "chat/domain/message.h"
#include "chat/domain/step.h"

namespace chat
{

class MessageRepository
{
public:
    virtual ~MessageRepository() = default;

    virtual std::vector<Message> findByConversation(const QUuid& conversationId) const = 0;
    virtual std::optional<Message> findById(const QUuid& id) const = 0;
    virtual int nextPosition(const QUuid& conversationId) const = 0;
    virtual bool saveMessage(const Message& message) = 0;
    virtual bool saveStep(const QUuid& messageId, const Step& step, const QDateTime& now) = 0;
    virtual bool removeSteps(const QUuid& messageId) = 0;
};

}
