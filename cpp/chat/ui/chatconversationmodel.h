#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <vector>

#include "chat/domain/conversation.h"

namespace chat
{

class ChatConversationModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        ConversationIdRole = Qt::UserRole + 1,
        TitleRole,
        CreatedAtRole,
        UpdatedAtRole
    };

    explicit ChatConversationModel(QObject* parent = nullptr);

    void reset(const std::vector<Conversation>& conversations);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<Conversation> m_conversations;
};

}
