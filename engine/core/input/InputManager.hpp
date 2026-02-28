#pragma once
#include <array>
#include "GtsKey.h"
#include "IInputSource.hpp"

#include <iostream>

// manage inputs on a frame/tick basis instead of an event basis
class InputManager : public IInputSource
{
    private:
        static constexpr size_t KEY_COUNT = static_cast<size_t>(GtsKey::COUNT);
        std::array<bool, KEY_COUNT> currentState{};
        std::array<bool, KEY_COUNT> previousState{};

        // Output windows can call the onKeyEvent
        friend class OutputWindow;
        void onKeyEvent(GtsKey key, bool pressed)
        {
            size_t idx = static_cast<size_t>(key);
            if (idx < KEY_COUNT)
                currentState[idx] = pressed;
        }

        // only the main engine can signal the start of a new frame
        friend class GravitasEngine;
        void beginFrame()
        {
            previousState = currentState;
        }

    public:
        // check if a key is currently being held down
        bool isKeyDown(GtsKey key) const override
        {
            size_t idx = static_cast<size_t>(key);
            return idx < KEY_COUNT && currentState[idx];
        }

        // check if a key is pressed on this update tick
        bool isKeyPressed(GtsKey key) const override
        {
            size_t idx = static_cast<size_t>(key);

            if (idx >= KEY_COUNT)
            {
                return false;
            }

            return currentState[idx] && !previousState[idx];
        }

        // check if a key was released this update tick
        bool isKeyReleased(GtsKey key) const override
        {
            size_t idx = static_cast<size_t>(key);

            if (idx >= KEY_COUNT)
            {
                return false;
            }

            return !currentState[idx] && previousState[idx];
        }

        // reset the input manager completely
        void reset()
        {
            currentState.fill(false);
            previousState.fill(false);
        }
};
