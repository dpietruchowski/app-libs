#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <QPointer>

#include "eventbus/eventbase.h"

class QObject;

class EventBus final
{
public:
    EventBus() = default;

    void publish(std::shared_ptr<EventBase> event);

    template <typename TEvent> void subscribe(std::function<void(const TEvent&)> handler)
    {
        static_assert(std::is_base_of_v<EventBase, TEvent>, "TEvent must derive from EventBase");
        m_handlers[std::type_index(typeid(TEvent))].push_back(
            Subscription { false, nullptr, [handler = std::move(handler)](const EventBase& event)
                           { handler(static_cast<const TEvent&>(event)); } });
    }

    template <typename TEvent>
    void subscribe(QObject* context, std::function<void(const TEvent&)> handler)
    {
        static_assert(std::is_base_of_v<EventBase, TEvent>, "TEvent must derive from EventBase");
        m_handlers[std::type_index(typeid(TEvent))].push_back(
            Subscription { true, context, [handler = std::move(handler)](const EventBase& event)
                           { handler(static_cast<const TEvent&>(event)); } });
    }

private:
    struct Subscription
    {
        bool boundToContext;
        QPointer<QObject> context;
        std::function<void(const EventBase&)> handler;
    };

    std::unordered_map<std::type_index, std::vector<Subscription>> m_handlers;
};
