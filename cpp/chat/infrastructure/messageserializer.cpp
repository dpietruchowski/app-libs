#include "chat/infrastructure/messageserializer.h"

#include "chat/infrastructure/jsoncodec.h"
#include "dbtoolkit/sqlcodec.h"

using namespace sql_codec;

namespace chat
{

const QStringList& MessageSerializer::columns()
{
    static const QStringList list { kId,     kConversationId, kPosition, kRole,     kText,
                                    kStatus, kError,          kContext,  kCreatedAt };
    return list;
}

QVariantMap MessageSerializer::toRow(const Message& message)
{
    return {
        { kId, toSql(message.id()) },
        { kConversationId, toSql(message.conversationId()) },
        { kPosition, message.position() },
        { kRole, toSql(message.role()) },
        { kText, toSql(message.text()) },
        { kStatus, toSql(message.status()) },
        { kError, message.error().isEmpty() ? QVariant() : QVariant(message.error()) },
        { kContext, message.context() ? json_codec::contextToSql(*message.context()) : QVariant() },
        { kCreatedAt, toSql(message.createdAt()) },
    };
}

Message MessageSerializer::fromRow(const QVariantMap& row, const std::vector<Step>& steps)
{
    return Message(uuidFromSql(row.value(kId)),
                   Message::Data {
                       uuidFromSql(row.value(kConversationId)), row.value(kPosition).toInt(),
                       enumFromSql<Role>(row.value(kRole)), row.value(kText).toString(),
                       enumFromSql<MessageStatus>(row.value(kStatus)), row.value(kError).toString(),
                       json_codec::contextFromSql(row.value(kContext)), steps },
                   dateTimeFromSql(row.value(kCreatedAt)));
}

}
