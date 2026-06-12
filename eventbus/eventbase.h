#pragma once

#include <QUuid>

class EventBase
{
public:
    EventBase();
    virtual ~EventBase() = default;

    const QUuid& id() const;

private:
    QUuid m_id;
};
