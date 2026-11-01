#include "chat/infrastructure/messagerepositorydb.h"

#include <map>

#include "chat/infrastructure/conversationserializer.h"
#include "chat/infrastructure/messageserializer.h"
#include "chat/infrastructure/stepserializer.h"
#include "dbtoolkit/dbstorage.h"
#include "dbtoolkit/query/column.h"
#include "dbtoolkit/query/createindex.h"
#include "dbtoolkit/query/createtable.h"
#include "dbtoolkit/query/order.h"
#include "dbtoolkit/query/select.h"
#include "dbtoolkit/query/where.h"
#include "dbtoolkit/sqlcodec.h"

using namespace sql_codec;

namespace chat
{

MessageRepositoryDb::MessageRepositoryDb(DbStorage& storage)
    : m_storage(storage)
    , m_messages(MessageSerializer::kTable, MessageSerializer::kId, MessageSerializer::columns(),
                 storage)
    , m_steps(StepSerializer::kTable, StepSerializer::kId, StepSerializer::columns(), storage)
{
}

bool MessageRepositoryDb::createTable()
{
    CreateTable messages(MessageSerializer::kTable);
    messages.ifNotExists()
        .column(Column(MessageSerializer::kId).blob().primaryKey().notNull())
        .column(Column(MessageSerializer::kConversationId).blob().notNull())
        .column(Column(MessageSerializer::kPosition).integer().notNull())
        .column(Column(MessageSerializer::kRole).text().notNull())
        .column(Column(MessageSerializer::kText).text().notNull().defaultValue(""))
        .column(Column(MessageSerializer::kStatus).text().notNull())
        .column(Column(MessageSerializer::kError).text())
        .column(Column(MessageSerializer::kContext).text())
        .column(Column(MessageSerializer::kCreatedAt).text().notNull())
        .foreignKey(MessageSerializer::kConversationId, ConversationSerializer::kTable,
                    ConversationSerializer::kId, OnDeleteAction::Cascade);

    CreateIndex messagesByPosition(QStringLiteral("chat_message_by_position"),
                                   MessageSerializer::kTable);
    messagesByPosition.ifNotExists().unique().columns(
        { MessageSerializer::kConversationId, MessageSerializer::kPosition });

    CreateTable steps(StepSerializer::kTable);
    steps.ifNotExists()
        .column(Column(StepSerializer::kId).blob().primaryKey().notNull())
        .column(Column(StepSerializer::kMessageId).blob().notNull())
        .column(Column(StepSerializer::kParentStepId).blob())
        .column(Column(StepSerializer::kPosition).integer().notNull())
        .column(Column(StepSerializer::kKind).text().notNull())
        .column(Column(StepSerializer::kAgent).text().notNull().defaultValue(""))
        .column(Column(StepSerializer::kName).text())
        .column(Column(StepSerializer::kInput).text())
        .column(Column(StepSerializer::kOutput).text())
        .column(Column(StepSerializer::kStatus).text().notNull())
        .column(Column(StepSerializer::kStartedAt).text())
        .column(Column(StepSerializer::kFinishedAt).text())
        .foreignKey(StepSerializer::kMessageId, MessageSerializer::kTable, MessageSerializer::kId,
                    OnDeleteAction::Cascade)
        .foreignKey(StepSerializer::kParentStepId, StepSerializer::kTable, StepSerializer::kId,
                    OnDeleteAction::Cascade);

    CreateIndex stepsByPosition(QStringLiteral("chat_step_by_position"), StepSerializer::kTable);
    stepsByPosition.ifNotExists().unique().columns(
        { StepSerializer::kMessageId, StepSerializer::kPosition });

    return m_messages.createTable(messages) && m_storage.execute(messagesByPosition).toInt() == 1
        && m_steps.createTable(steps) && m_storage.execute(stepsByPosition).toInt() == 1;
}

std::vector<Message> MessageRepositoryDb::assemble(const QVector<QVariantMap>& messageRows,
                                                   const QVector<QVariantMap>& stepRows)
{
    std::map<QUuid, std::vector<Step>> stepsByMessage;
    for (const QVariantMap& row : stepRows)
    {
        stepsByMessage[uuidFromSql(row.value(StepSerializer::kMessageId))].push_back(
            StepSerializer::fromRow(row));
    }

    std::vector<Message> messages;
    messages.reserve(static_cast<size_t>(messageRows.size()));
    for (const QVariantMap& row : messageRows)
    {
        messages.push_back(MessageSerializer::fromRow(
            row, stepsByMessage[uuidFromSql(row.value(MessageSerializer::kId))]));
    }
    return messages;
}

std::vector<Message> MessageRepositoryDb::findByConversation(const QUuid& conversationId) const
{
    const Where inConversation
        = Where(MessageSerializer::kConversationId).equals(toSql(conversationId));

    Select conversationMessages({ MessageSerializer::kId });
    conversationMessages.from(MessageSerializer::kTable).where(inConversation);

    return assemble(m_messages.select(inConversation, Order(MessageSerializer::kPosition).asc()),
                    m_steps.select(Where(StepSerializer::kMessageId).in(conversationMessages),
                                   Order(StepSerializer::kPosition).asc()));
}

std::optional<Message> MessageRepositoryDb::findById(const QUuid& id) const
{
    const QVector<QVariantMap> messageRows
        = m_messages.select(Where(MessageSerializer::kId).equals(toSql(id)), Order(), 1);
    if (messageRows.isEmpty())
    {
        return std::nullopt;
    }

    const QVector<QVariantMap> stepRows
        = m_steps.select(Where(StepSerializer::kMessageId).equals(toSql(id)),
                         Order(StepSerializer::kPosition).asc());
    return assemble(messageRows, stepRows).front();
}

int MessageRepositoryDb::nextPosition(const QUuid& conversationId) const
{
    const QVector<QVariantMap> last
        = m_messages.select(Where(MessageSerializer::kConversationId).equals(toSql(conversationId)),
                            Order(MessageSerializer::kPosition).desc(), 1);
    return last.isEmpty() ? 0 : last.first().value(MessageSerializer::kPosition).toInt() + 1;
}

bool MessageRepositoryDb::saveMessage(const Message& message)
{
    return m_messages.upsert(MessageSerializer::toRow(message)).isValid();
}

bool MessageRepositoryDb::saveStep(const QUuid& messageId, const Step& step, const QDateTime& now)
{
    const QVector<QVariantMap> existing
        = m_steps.select(Where(StepSerializer::kId).equals(toSql(step.id)), Order(), 1);

    StepSerializer::Timing timing;
    timing.finishedAt = step.status == StepStatus::Running ? QDateTime() : now;
    if (existing.isEmpty())
    {
        timing.position = m_steps.count(Where(StepSerializer::kMessageId).equals(toSql(messageId)));
        timing.startedAt = now;
    }
    else
    {
        timing.position = existing.first().value(StepSerializer::kPosition).toInt();
        timing.startedAt = dateTimeFromSql(existing.first().value(StepSerializer::kStartedAt));
    }

    return m_steps.upsert(StepSerializer::toRow(messageId, step, timing)).isValid();
}

bool MessageRepositoryDb::removeSteps(const QUuid& messageId)
{
    return m_steps.remove(Where(StepSerializer::kMessageId).equals(toSql(messageId))) >= 0;
}

}
