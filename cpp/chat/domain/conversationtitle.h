#pragma once

#include <QString>

namespace chat
{

inline constexpr int kMaxConversationTitleLength = 40;

QString conversationTitle(const QString& firstMessage);

}
