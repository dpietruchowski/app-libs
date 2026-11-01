#pragma once

#include <QDateTime>
#include <QString>
#include <QUuid>
#include <optional>

#include "chat/domain/chatcontext.h"

namespace chat
{

class Conversation final
{
public:
    struct Data final
    {
        QString title;
        std::optional<ChatContext> origin;

        bool operator==(const Data& other) const = default;
    };

    Conversation() = default;

    Conversation(const QUuid& id, const Data& data, const QDateTime& createdAt,
                 const QDateTime& updatedAt)
        : m_id(id)
        , m_data(data)
        , m_createdAt(createdAt)
        , m_updatedAt(updatedAt)
    {
    }

    static Conversation create(const Data& data, const QDateTime& now)
    {
        return Conversation(QUuid::createUuid(), data, now, now);
    }

    const QUuid& id() const { return m_id; }
    const Data& data() const { return m_data; }
    const QString& title() const { return m_data.title; }
    const std::optional<ChatContext>& origin() const { return m_data.origin; }
    const QDateTime& createdAt() const { return m_createdAt; }
    const QDateTime& updatedAt() const { return m_updatedAt; }

    void touch(const QDateTime& now) { m_updatedAt = now; }

    bool operator==(const Conversation& other) const = default;

private:
    QUuid m_id;
    Data m_data;
    QDateTime m_createdAt;
    QDateTime m_updatedAt;
};

}
