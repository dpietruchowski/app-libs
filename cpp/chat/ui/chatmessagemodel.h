#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariant>
#include <functional>
#include <optional>
#include <vector>

#include "chat/domain/message.h"
#include "chat/domain/step.h"

namespace chat
{

class AgentCatalog;

class ChatMessageModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        AuthorRole = Qt::UserRole + 1,
        TextRole,
        StatusRole,
        ErrorRole,
        StepsRole,
        CreatedAtRole
    };

    explicit ChatMessageModel(const AgentCatalog& catalog, QObject* parent = nullptr);

    void setLoginCommand(const QString& command);
    void reset(const std::vector<Message>& messages);
    bool setRunningText(const QString& text);
    bool putRunningStep(const Step& step);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    std::optional<int> runningRow() const;
    bool editRunning(const std::function<bool(Message::Data&)>& edit, int changedRole);
    QVariantList stepsOf(const Message& message) const;

    const AgentCatalog& m_catalog;
    QString m_loginCommand;
    std::vector<Message> m_messages;
};

}
