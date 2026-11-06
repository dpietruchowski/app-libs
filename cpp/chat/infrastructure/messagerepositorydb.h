#pragma once

#include <QVariantMap>
#include <QVector>
#include <optional>
#include <vector>

#include "chat/domain/messagerepository.h"
#include "dbtoolkit/dbrepository.h"

class DbStorage;

namespace chat
{

class MessageRepositoryDb final : public MessageRepository
{
public:
    explicit MessageRepositoryDb(DbStorage& storage);

    bool createTable();

    std::vector<Message> findByConversation(const QUuid& conversationId) const override;
    std::optional<Message> findById(const QUuid& id) const override;
    int nextPosition(const QUuid& conversationId) const override;
    bool saveMessage(const Message& message) override;
    bool saveStep(const QUuid& messageId, const Step& step, const QDateTime& now) override;
    bool removeSteps(const QUuid& messageId) override;

private:
    static std::vector<Message> assemble(const QVector<QVariantMap>& messageRows,
                                         const QVector<QVariantMap>& stepRows);

    DbStorage& m_storage;
    DbRepository m_messages;
    DbRepository m_steps;
};

}
