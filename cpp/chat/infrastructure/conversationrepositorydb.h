#pragma once

#include <optional>
#include <vector>

#include "chat/domain/conversationrepository.h"
#include "dbtoolkit/dbrepository.h"

class DbStorage;

namespace chat
{

class ConversationRepositoryDb final : public ConversationRepository
{
public:
    explicit ConversationRepositoryDb(DbStorage& storage);

    bool createTable();

    std::vector<Conversation> findAll() const override;
    std::optional<Conversation> findById(const QUuid& id) const override;
    bool save(const Conversation& conversation) override;
    bool remove(const QUuid& id) override;

private:
    DbStorage& m_storage;
    DbRepository m_conversations;
};

}
