#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "chat/domain/conversation.h"

namespace chat
{

class ConversationSerializer final
{
public:
    static inline const QString kTable = QStringLiteral("chat_conversation");
    static inline const QString kId = QStringLiteral("id");
    static inline const QString kTitle = QStringLiteral("title");
    static inline const QString kCreatedAt = QStringLiteral("created_at");
    static inline const QString kUpdatedAt = QStringLiteral("updated_at");
    static inline const QString kContextKey = QStringLiteral("context_key");
    static inline const QString kContextTitle = QStringLiteral("context_title");
    static inline const QString kContextData = QStringLiteral("context_data_json");
    static inline const QString kBackendSession = QStringLiteral("backend_session");
    static inline const QString kBackendSessionStart = QStringLiteral("backend_session_start");

    static const QStringList& columns();

    static QVariantMap toRow(const Conversation& conversation);
    static Conversation fromRow(const QVariantMap& row);
};

}
