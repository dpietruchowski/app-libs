#include "chat/infrastructure/conversationrepositorydb.h"

#include "chat/infrastructure/conversationserializer.h"
#include "dbtoolkit/dbstorage.h"
#include "dbtoolkit/query/column.h"
#include "dbtoolkit/query/createindex.h"
#include "dbtoolkit/query/createtable.h"
#include "dbtoolkit/query/order.h"
#include "dbtoolkit/query/where.h"
#include "dbtoolkit/sqlcodec.h"

using namespace sql_codec;

namespace chat
{

ConversationRepositoryDb::ConversationRepositoryDb(DbStorage& storage)
    : m_storage(storage)
    , m_conversations(ConversationSerializer::kTable, ConversationSerializer::kId,
                      ConversationSerializer::columns(), storage)
{
}

bool ConversationRepositoryDb::createTable()
{
    CreateTable conversations(ConversationSerializer::kTable);
    conversations.ifNotExists()
        .column(Column(ConversationSerializer::kId).blob().primaryKey().notNull())
        .column(Column(ConversationSerializer::kTitle).text().notNull().defaultValue(""))
        .column(Column(ConversationSerializer::kCreatedAt).text().notNull())
        .column(Column(ConversationSerializer::kUpdatedAt).text().notNull())
        .column(Column(ConversationSerializer::kContextKey).text())
        .column(Column(ConversationSerializer::kContextTitle).text())
        .column(Column(ConversationSerializer::kContextData).text())
        .column(Column(ConversationSerializer::kBackendSession).text())
        .column(Column(ConversationSerializer::kBackendSessionStart).integer());

    CreateIndex byUpdate(QStringLiteral("chat_conversation_by_update"),
                         ConversationSerializer::kTable);
    byUpdate.ifNotExists().columns({ ConversationSerializer::kUpdatedAt });

    return m_conversations.createTable(conversations) && m_storage.execute(byUpdate).toInt() == 1;
}

std::vector<Conversation> ConversationRepositoryDb::findAll() const
{
    return m_conversations.selectAs<Conversation>(
        &ConversationSerializer::fromRow, Where(),
        Order(ConversationSerializer::kUpdatedAt).desc().then(QStringLiteral("rowid")).desc());
}

std::optional<Conversation> ConversationRepositoryDb::findById(const QUuid& id) const
{
    return m_conversations.findFirstAs<Conversation>(
        &ConversationSerializer::fromRow, Where(ConversationSerializer::kId).equals(toSql(id)));
}

bool ConversationRepositoryDb::save(const Conversation& conversation)
{
    return m_conversations.upsert(ConversationSerializer::toRow(conversation)).isValid();
}

bool ConversationRepositoryDb::remove(const QUuid& id)
{
    return m_conversations.remove(Where(ConversationSerializer::kId).equals(toSql(id))) >= 0;
}

}
