#include "eventbus/eventbus.h"

void EventBus::publish(std::shared_ptr<EventBase> event)
{
    if (!event)
    {
        return;
    }

    const EventBase& eventRef = *event;
    auto it = m_handlers.find(std::type_index(typeid(eventRef)));
    if (it != m_handlers.end())
    {
        for (const auto& handler : it->second)
        {
            handler(*event);
        }
    }
}
