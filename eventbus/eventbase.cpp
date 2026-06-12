#include "eventbus/eventbase.h"

EventBase::EventBase()
    : m_id(QUuid::createUuid())
{
}

const QUuid& EventBase::id() const { return m_id; }
