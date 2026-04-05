#pragma once

#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

class GtsEventBus
{
    private:
        using ErasedListener = std::function<void(const void*)>;
        using ErasedEventPtr = std::unique_ptr<void, void(*)(void*)>;

        struct QueuedEvent
        {
            ErasedEventPtr payload;
        };

        std::unordered_map<std::type_index, std::vector<ErasedListener>> listenersByType;
        std::unordered_map<std::type_index, std::vector<QueuedEvent>> queuedEventsByType;

    public:
        template<typename Event>
        void subscribe(std::function<void(const Event&)> listener)
        {
            auto& listeners = listenersByType[std::type_index(typeid(Event))];
            listeners.emplace_back(
                [listener = std::move(listener)](const void* payload)
                {
                    listener(*static_cast<const Event*>(payload));
                });
        }

        template<typename Event>
        void emit(const Event& event)
        {
            auto destroy = [](void* payload)
            {
                delete static_cast<Event*>(payload);
            };

            auto& queue = queuedEventsByType[std::type_index(typeid(Event))];
            queue.push_back(QueuedEvent{
                ErasedEventPtr(new Event(event), destroy)
            });
        }

        void dispatch()
        {
            auto queuedEvents = std::move(queuedEventsByType);
            queuedEventsByType.clear();

            for (auto& [eventType, events] : queuedEvents)
            {
                auto listenersIt = listenersByType.find(eventType);
                if (listenersIt == listenersByType.end())
                    continue;

                auto listenersSnapshot = listenersIt->second;
                for (const auto& queuedEvent : events)
                {
                    const void* payload = queuedEvent.payload.get();
                    for (const auto& listener : listenersSnapshot)
                    {
                        if (listener)
                            listener(payload);
                    }
                }
            }
        }
};
