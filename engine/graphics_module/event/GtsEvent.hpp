#pragma once
#include <functional>
#include <vector>
#include <algorithm>
#include <cstddef>

// use a pack expansion operator for a fairly simple event class, that will carry its weight with the multitude of events that are gonna be in this engine
template <typename... Args>
class GtsEvent
{
public:
    using Listener = std::function<void(Args...)>;
    using ListenerId = std::size_t;

    // subscribe to a listener, return an id which can be used to unsub
    ListenerId subscribe(Listener listener)
    {
        const ListenerId id = ++lastId;
        listeners.push_back({id, std::move(listener)});
        return id;
    }

    // unsub a listener by id
    void unsubscribe(ListenerId id)
    {
        listeners.erase(
            std::remove_if(listeners.begin(), listeners.end(),
                [id](const auto& pair) { return pair.first == id; }),
            listeners.end());
    }

    void notify(Args... args)
    {
        auto snapshot = listeners;
        for (auto& [id, listener] : snapshot)
        {
            if (listener) listener(args...);
        }
    }

    void clear()
    {
        listeners.clear();
    }

    bool empty() const
    {
        return listeners.empty();
    }

private:
    std::vector<std::pair<ListenerId, Listener>> listeners;
    ListenerId lastId = 0;
};


