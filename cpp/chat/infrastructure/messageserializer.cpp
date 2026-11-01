#include "chat/infrastructure/messageserializer.h"

#include "chat/infrastructure/enumcodec.h"
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
        { kRole, enum_codec::roleToSql(message.role()) },
        { kText, toSql(message.text()) },
        { kStatus, enum_codec::messageStatusToSql(message.status()) },
        { kError, message.error().isEmpty() ? QVariant() : QVariant(message.error()) },
        { kContext, message.context() ? json_codec::contextToSql(*message.context()) : QVariant() },
        { kCreatedAt, toSql(message.createdAt()) },
    };
}

Message MessageSerializer::fromRow(const QVariantMap& row, const std::vector<Step>& steps)
{
    return Message(
        uuidFromSql(row.value(kId)),
        Message::Data {
            uuidFromSql(row.value(kConversationId)), row.value(kPosition).toInt(),
            enum_codec::roleFromSql(row.value(kRole).toString()), row.value(kText).toString(),
            enum_codec::messageStatusFromSql(row.value(kStatus).toString()),
            row.value(kError).toString(), json_codec::contextFromSql(row.value(kContext)), steps },
        dateTimeFromSql(row.value(kCreatedAt)));
}

}
