#include "chat/ui/chatconversationmodel.h"

namespace chat
{

ChatConversationModel::ChatConversationModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void ChatConversationModel::reset(const std::vector<Conversation>& conversations)
{
    if (m_conversations == conversations)
    {
        return;
    }
    beginResetModel();
    m_conversations = conversations;
    endResetModel();
}

int ChatConversationModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_conversations.size());
}

QVariant ChatConversationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return QVariant();
    }

    const Conversation& conversation = m_conversations[static_cast<size_t>(index.row())];
    switch (role)
    {
        case ConversationIdRole:
            return conversation.id().toString(QUuid::WithoutBraces);
        case TitleRole:
            return conversation.title();
        case CreatedAtRole:
            return conversation.createdAt();
        case UpdatedAtRole:
            return conversation.updatedAt();
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> ChatConversationModel::roleNames() const
{
    return { { ConversationIdRole, "conversationId" },
             { TitleRole, "title" },
             { CreatedAtRole, "createdAt" },
             { UpdatedAtRole, "updatedAt" } };
}

}
