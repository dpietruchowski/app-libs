#pragma once

#include <QUuid>
#include <optional>
#include <vector>

#include "chat/domain/conversation.h"

namespace chat
{

class ConversationRepository
{
public:
    virtual ~ConversationRepository() = default;

    virtual std::vector<Conversation> findAll() const = 0;
    virtual std::optional<Conversation> findById(const QUuid& id) const = 0;
    virtual bool save(const Conversation& conversation) = 0;
    virtual bool remove(const QUuid& id) = 0;
};

}
