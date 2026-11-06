#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <vector>

#include "chat/domain/message.h"
#include "chat/domain/step.h"

namespace chat
{

class MessageSerializer final
{
public:
    static inline const QString kTable = QStringLiteral("chat_message");
    static inline const QString kId = QStringLiteral("id");
    static inline const QString kConversationId = QStringLiteral("conversation_id");
    static inline const QString kPosition = QStringLiteral("position");
    static inline const QString kRole = QStringLiteral("role");
    static inline const QString kText = QStringLiteral("text");
    static inline const QString kStatus = QStringLiteral("status");
    static inline const QString kError = QStringLiteral("error");
    static inline const QString kContext = QStringLiteral("context_json");
    static inline const QString kCreatedAt = QStringLiteral("created_at");

    static const QStringList& columns();

    static QVariantMap toRow(const Message& message);
    static Message fromRow(const QVariantMap& row, const std::vector<Step>& steps);
};

}
