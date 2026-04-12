#pragma once

#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

#include "SubscriptionToken.hpp"

// Event bus for platform-layer events: window resize, raw key input from GLFW,
// frame-ended notifications from the renderer, etc.
//
// These events originate from OS/GPU callbacks (glfwPollEvents, Vulkan present)
// that fire outside the game loop. Events are queued on emit() and delivered
// to all subscribers on the next dispatch() call, which happens once per frame
// in GtsPlatform::beginFrame() — always on the main thread.
//
// Use this bus for infrastructure events that cross the platform boundary.
// For ECS gameplay events between game systems, use ECSWorld::publish/subscribe.
//
// Subscription lifetime: subscribe() returns a SubscriptionToken. Hold it as a
// member; the subscription is removed when the token is destroyed. The token is
// safe to destroy after the bus — the weak sentinel prevents a use-after-free.
class GtsPlatformEventBus
{
    // Sentinel shared with every SubscriptionToken lambda. When the bus is
    // destroyed, m_alive expires and token destructors skip the unsubscribe.
    // Declared first so it is destroyed last (reverse declaration order).
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

    using ListenerEntry = std::pair<uint64_t, std::function<void(const void*)>>;
    std::unordered_map<std::type_index, std::vector<ListenerEntry>> listeners;
    std::unordered_map<std::type_index, std::vector<std::unique_ptr<void, void(*)(void*)>>> queue;
    uint64_t nextId = 1;

public:
    // Queue an event for delivery on the next dispatch() call.
    // Safe to call from GLFW/Vulkan callbacks.
    template<typename Event>
    void emit(Event&& event)
    {
        using D = std::decay_t<Event>;
        const std::type_index key = std::type_index(typeid(D));

        auto destroy = [](void* p) { delete static_cast<D*>(p); };
        queue[key].emplace_back(
            new D(std::forward<Event>(event)),
            destroy);
    }

    // Deliver all queued events to subscribers, then clear the queue.
    // Call once per frame from the main thread (GtsPlatform::beginFrame).
    void dispatch()
    {
        auto pending = std::move(queue);
        queue.clear();

        for (auto& [key, events] : pending)
        {
            auto it = listeners.find(key);
            if (it == listeners.end())
                continue;

            // Snapshot so subscribe/unsubscribe during dispatch is safe.
            const auto snapshot = it->second;
            for (const auto& event : events)
            {
                const void* payload = event.get();
                for (const auto& [id, fn] : snapshot)
                    fn(payload);
            }
        }
    }

    // Subscribe to event type E. Returns a SubscriptionToken that unsubscribes
    // on destruction. Store the token as a member of the subscribing object.
    template<typename E>
    SubscriptionToken subscribe(std::function<void(const E&)> handler)
    {
        const std::type_index key = std::type_index(typeid(E));
        const uint64_t id = nextId++;

        listeners[key].emplace_back(id,
            [h = std::move(handler)](const void* payload)
            {
                h(*static_cast<const E*>(payload));
            });

        std::weak_ptr<bool> weakAlive = m_alive;
        return SubscriptionToken([this, key, id, weakAlive = std::move(weakAlive)]()
        {
            if (weakAlive.expired())
                return;

            auto it = listeners.find(key);
            if (it == listeners.end())
                return;

            auto& vec = it->second;
            vec.erase(
                std::remove_if(vec.begin(), vec.end(),
                    [id](const ListenerEntry& e) { return e.first == id; }),
                vec.end());
        });
    }
};
