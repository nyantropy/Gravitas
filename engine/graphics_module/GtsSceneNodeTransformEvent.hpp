#pragma once

//an event that gets fired when a transform happens
#include <vector>
#include <functional>

class GtsSceneNodeTransformEvent
{
    public:
        using Listener = std::function<void()>;

        void subscribe(Listener listener)
        {
            listeners.push_back(listener);
        }

        void notify()
        {
            for (auto& listener : listeners)
            {
                listener();
            }
        }

    private:
        std::vector<Listener> listeners;
};