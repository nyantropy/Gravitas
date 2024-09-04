#pragma once

#include <vector>
#include <functional>
#include <cstdint>

class GtsOnFrameEndedEvent
{
    public:
        using Listener = std::function<void(int, uint32_t)>;

        void subscribe(Listener listener)
        {
            listeners.push_back(listener);
        }

        void notify(int deltaTime, uint32_t imageIndex)
        {
            for (auto& listener : listeners)
            {
                listener(deltaTime, imageIndex);
            }
        }

    private:
        std::vector<Listener> listeners;
};