#include "eventbus/eventbus.h"

#include <QMetaObject>
#include <QObject>
#include <QThread>

void EventBus::publish(std::shared_ptr<EventBase> event)
{
    if (!event)
    {
        return;
    }

    const EventBase& eventRef = *event;
    auto it = m_handlers.find(std::type_index(typeid(eventRef)));
    if (it == m_handlers.end())
    {
        return;
    }

    for (const auto& subscription : it->second)
    {
        if (!subscription.boundToContext)
        {
            subscription.handler(*event);
            continue;
        }

        QObject* context = subscription.context;
        if (!context)
        {
            continue;
        }

        if (context->thread() == QThread::currentThread())
        {
            subscription.handler(*event);
            continue;
        }

        QMetaObject::invokeMethod(
            context, [event, handler = subscription.handler]() { handler(*event); },
            Qt::QueuedConnection);
    }
}
