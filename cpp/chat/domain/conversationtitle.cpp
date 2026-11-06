#include "chat/domain/conversationtitle.h"

#include <QChar>

namespace chat
{

QString conversationTitle(const QString& firstMessage)
{
    const QString text = firstMessage.simplified();
    if (text.size() <= kMaxConversationTitleLength)
    {
        return text;
    }
    return text.left(kMaxConversationTitleLength - 1).trimmed() + QChar(0x2026);
}

}
