#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "eventbus/eventbase.h"

class EventBus final
{
public:
    EventBus() = default;

    void publish(std::shared_ptr<EventBase> event);

    template <typename TEvent> void subscribe(std::function<void(const TEvent&)> handler)
    {
        static_assert(std::is_base_of_v<EventBase, TEvent>, "TEvent must derive from EventBase");
        m_handlers[std::type_index(typeid(TEvent))].push_back(
            [handler = std::move(handler)](const EventBase& event)
            { handler(static_cast<const TEvent&>(event)); });
    }

private:
    std::unordered_map<std::type_index, std::vector<std::function<void(const EventBase&)>>>
        m_handlers;
};
