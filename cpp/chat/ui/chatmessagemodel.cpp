#include "chat/ui/chatmessagemodel.h"

#include <QVariantMap>
#include <algorithm>

#include "chat/application/agentcatalog.h"
#include "chat/domain/enumnames.h"
#include "chat/ui/errortext.h"

namespace chat
{

ChatMessageModel::ChatMessageModel(const AgentCatalog& catalog, QObject* parent)
    : QAbstractListModel(parent)
    , m_catalog(catalog)
{
}

void ChatMessageModel::setLoginCommand(const QString& command)
{
    if (m_loginCommand == command)
    {
        return;
    }
    m_loginCommand = command;
    if (!m_messages.empty())
    {
        emit dataChanged(index(0), index(rowCount() - 1), { ErrorRole });
    }
}

void ChatMessageModel::reset(const std::vector<Message>& messages)
{
    beginResetModel();
    m_messages = messages;
    endResetModel();
}

bool ChatMessageModel::setRunningText(const QString& text)
{
    return editRunning(
        [&text](Message::Data& data)
        {
            if (data.text == text)
            {
                return false;
            }
            data.text = text;
            return true;
        },
        TextRole);
}

bool ChatMessageModel::putRunningStep(const Step& step)
{
    return editRunning(
        [&step](Message::Data& data)
        {
            const auto known = std::find_if(data.steps.begin(), data.steps.end(),
                                            [&step](const Step& s) { return s.id == step.id; });
            if (known == data.steps.end())
            {
                data.steps.push_back(step);
            }
            else
            {
                *known = step;
            }
            return true;
        },
        StepsRole);
}

int ChatMessageModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_messages.size());
}

QVariant ChatMessageModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return QVariant();
    }

    const Message& message = m_messages[static_cast<size_t>(index.row())];
    switch (role)
    {
        case AuthorRole:
            return roleName(message.role());
        case TextRole:
            return message.text();
        case StatusRole:
            return statusName(message.status());
        case ErrorRole:
            return errorText(message.status(), message.error(), m_loginCommand);
        case StepsRole:
            return stepsOf(message);
        case CreatedAtRole:
            return message.createdAt();
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> ChatMessageModel::roleNames() const
{
    return { { AuthorRole, "role" }, { TextRole, "text" },   { StatusRole, "status" },
             { ErrorRole, "error" }, { StepsRole, "steps" }, { CreatedAtRole, "createdAt" } };
}

std::optional<int> ChatMessageModel::runningRow() const
{
    for (int row = rowCount() - 1; row >= 0; --row)
    {
        if (m_messages[static_cast<size_t>(row)].status() == MessageStatus::Running)
        {
            return row;
        }
    }
    return std::nullopt;
}

bool ChatMessageModel::editRunning(const std::function<bool(Message::Data&)>& edit, int changedRole)
{
    const std::optional<int> row = runningRow();
    if (!row)
    {
        return false;
    }

    Message& message = m_messages[static_cast<size_t>(*row)];
    Message::Data data = message.data();
    if (!edit(data))
    {
        return false;
    }

    message = Message(message.id(), data, message.createdAt());
    emit dataChanged(index(*row), index(*row), { changedRole });
    return true;
}

QVariantList ChatMessageModel::stepsOf(const Message& message) const
{
    QVariantList steps;
    for (const Step& step : message.steps())
    {
        const AppAgent* entry = m_catalog.find(step.agent);
        steps.append(QVariantMap {
            { QStringLiteral("id"), step.id.toString(QUuid::WithoutBraces) },
            { QStringLiteral("parentId"),
              step.parentId ? step.parentId->toString(QUuid::WithoutBraces) : QString() },
            { QStringLiteral("kind"), stepKindName(step.kind) },
            { QStringLiteral("agent"), step.agent },
            { QStringLiteral("agentTitle"), entry ? entry->title : step.agent },
            { QStringLiteral("tool"), step.tool },
            { QStringLiteral("status"), stepStatusName(step.status) },
            { QStringLiteral("effect"), effectName(step.effect) } });
    }
    return steps;
}

}
