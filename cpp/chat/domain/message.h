#pragma once

#include <QDateTime>
#include <QString>
#include <QUuid>
#include <optional>
#include <vector>

#include "chat/domain/chatcontext.h"
#include "chat/domain/enums.h"
#include "chat/domain/step.h"

namespace chat
{

class Message final
{
public:
    struct Data final
    {
        QUuid conversationId;
        int position { 0 };
        Role role { Role::User };
        QString text;
        MessageStatus status { MessageStatus::Complete };
        QString error;
        std::optional<ChatContext> context;
        std::vector<Step> steps;

        bool operator==(const Data& other) const = default;
    };

    Message() = default;

    Message(const QUuid& id, const Data& data, const QDateTime& createdAt)
        : m_id(id)
        , m_data(data)
        , m_createdAt(createdAt)
    {
    }

    static Message create(const Data& data, const QDateTime& now)
    {
        return Message(QUuid::createUuid(), data, now);
    }

    const QUuid& id() const { return m_id; }
    const Data& data() const { return m_data; }
    const QUuid& conversationId() const { return m_data.conversationId; }
    int position() const { return m_data.position; }
    Role role() const { return m_data.role; }
    const QString& text() const { return m_data.text; }
    MessageStatus status() const { return m_data.status; }
    const QString& error() const { return m_data.error; }
    const std::optional<ChatContext>& context() const { return m_data.context; }
    const std::vector<Step>& steps() const { return m_data.steps; }
    const QDateTime& createdAt() const { return m_createdAt; }

    bool operator==(const Message& other) const = default;

private:
    QUuid m_id;
    Data m_data;
    QDateTime m_createdAt;
};

}
