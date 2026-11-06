#pragma once

#include <QVariant>
#include <optional>

#include "chat/domain/chatcontext.h"

namespace chat::json_codec
{

QVariant contextToSql(const ChatContext& context);
std::optional<ChatContext> contextFromSql(const QVariant& value);

}
