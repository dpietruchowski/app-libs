#include "chat/infrastructure/conversationserializer.h"

#include <optional>

#include "chat/infrastructure/jsoncodec.h"
#include "dbtoolkit/sqlcodec.h"

using namespace sql_codec;

namespace chat
{

const QStringList& ConversationSerializer::columns()
{
    static const QStringList list { kId,          kTitle,          kCreatedAt,
                                    kUpdatedAt,   kContextKey,     kContextTitle,
                                    kContextData, kBackendSession, kBackendSessionStart };
    return list;
}

QVariantMap ConversationSerializer::toRow(const Conversation& conversation)
{
    QVariantMap row {
        { kId, toSql(conversation.id()) },
        { kTitle, toSql(conversation.title()) },
        { kCreatedAt, toSql(conversation.createdAt()) },
        { kUpdatedAt, toSql(conversation.updatedAt()) },
        { kContextKey, QVariant() },
        { kContextTitle, QVariant() },
        { kContextData, QVariant() },
        { kBackendSession, QVariant() },
        { kBackendSessionStart, QVariant() },
    };

    if (const std::optional<ChatContext>& origin = conversation.origin())
    {
        row[kContextKey] = toSql(origin->key);
        row[kContextTitle] = toSql(origin->title);
        row[kContextData] = json_codec::objectToSql(origin->data);
    }
    if (const std::optional<BackendSession>& session = conversation.backendSession())
    {
        row[kBackendSession] = session->id;
        row[kBackendSessionStart] = session->start;
    }
    return row;
}

Conversation ConversationSerializer::fromRow(const QVariantMap& row)
{
    std::optional<ChatContext> origin;
    if (!row.value(kContextKey).isNull())
    {
        origin
            = ChatContext { row.value(kContextKey).toString(), row.value(kContextTitle).toString(),
                            json_codec::objectFromSql(row.value(kContextData)) };
    }

    std::optional<BackendSession> session;
    if (!row.value(kBackendSession).isNull())
    {
        session = BackendSession { row.value(kBackendSession).toString(),
                                   row.value(kBackendSessionStart).toInt() };
    }

    return Conversation(
        uuidFromSql(row.value(kId)), Conversation::Data { row.value(kTitle).toString(), origin },
        dateTimeFromSql(row.value(kCreatedAt)), dateTimeFromSql(row.value(kUpdatedAt)), session);
}

}
