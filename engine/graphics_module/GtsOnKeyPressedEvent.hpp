#include <vector>
#include <functional>

class GtsOnKeyPressedEvent
{
    public:
        using Listener = std::function<void(int key, int scancode, int action, int mods)>;

        void subscribe(Listener listener)
        {
            listeners.push_back(listener);
        }

        void notify(int key, int scancode, int action, int mods)
        {
            for (auto& listener : listeners)
            {
                listener(key, scancode, action, mods);
            }
        }

    private:
        std::vector<Listener> listeners;
};
