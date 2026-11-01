#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>
#include <optional>

#include "chat/domain/chatcontext.h"

namespace chat::json_codec
{

QVariant objectToSql(const QJsonObject& object);
QJsonObject objectFromSql(const QVariant& value);

QVariant valueToSql(const QJsonValue& value);
QJsonValue valueFromSql(const QVariant& value);

QVariant contextToSql(const ChatContext& context);
std::optional<ChatContext> contextFromSql(const QVariant& value);

}
