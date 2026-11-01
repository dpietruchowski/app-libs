#include "chat/ui/chatconversationmodel.h"

#include <QList>
#include <algorithm>

namespace chat
{

namespace
{

    bool contains(const std::vector<Conversation>& conversations, const QUuid& id)
    {
        return std::any_of(conversations.begin(), conversations.end(),
                           [&id](const Conversation& conversation)
                           { return conversation.id() == id; });
    }

}

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

void ChatConversationModel::update(const std::vector<Conversation>& conversations)
{
    removeMissing(conversations);
    arrange(conversations);
    refreshChanged(conversations);
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

int ChatConversationModel::rowOf(const QUuid& id, int fromRow) const
{
    for (int row = fromRow; row < rowCount(); ++row)
    {
        if (m_conversations[static_cast<size_t>(row)].id() == id)
        {
            return row;
        }
    }
    return -1;
}

void ChatConversationModel::removeMissing(const std::vector<Conversation>& conversations)
{
    for (int row = rowCount() - 1; row >= 0; --row)
    {
        const auto position = m_conversations.begin() + row;
        if (contains(conversations, position->id()))
        {
            continue;
        }
        beginRemoveRows(QModelIndex(), row, row);
        m_conversations.erase(position);
        endRemoveRows();
    }
}

void ChatConversationModel::arrange(const std::vector<Conversation>& conversations)
{
    const int wanted = static_cast<int>(conversations.size());
    int row = 0;
    while (row < wanted)
    {
        const QUuid& id = conversations[static_cast<size_t>(row)].id();
        const int from = rowOf(id, row);
        if (from == row)
        {
            ++row;
            continue;
        }

        if (from > row)
        {
            beginMoveRows(QModelIndex(), from, from, QModelIndex(), row);
            std::rotate(m_conversations.begin() + row, m_conversations.begin() + from,
                        m_conversations.begin() + from + 1);
            endMoveRows();
            ++row;
            continue;
        }

        int last = row;
        while (last + 1 < wanted
               && !contains(m_conversations, conversations[static_cast<size_t>(last + 1)].id()))
        {
            ++last;
        }
        beginInsertRows(QModelIndex(), row, last);
        m_conversations.insert(m_conversations.begin() + row, conversations.begin() + row,
                               conversations.begin() + last + 1);
        endInsertRows();
        row = last + 1;
    }
}

void ChatConversationModel::refreshChanged(const std::vector<Conversation>& conversations)
{
    for (int row = 0; row < rowCount(); ++row)
    {
        Conversation& existing = m_conversations[static_cast<size_t>(row)];
        const Conversation& updated = conversations[static_cast<size_t>(row)];
        if (existing == updated)
        {
            continue;
        }

        QList<int> changedRoles;
        if (existing.title() != updated.title())
        {
            changedRoles.append(TitleRole);
        }
        if (existing.createdAt() != updated.createdAt())
        {
            changedRoles.append(CreatedAtRole);
        }
        if (existing.updatedAt() != updated.updatedAt())
        {
            changedRoles.append(UpdatedAtRole);
        }
        existing = updated;
        if (!changedRoles.isEmpty())
        {
            emit dataChanged(index(row), index(row), changedRoles);
        }
    }
}

}
