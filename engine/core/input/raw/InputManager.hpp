#pragma once

#include <array>

#include "InputBinding.h"
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
        ModifierFlags currentModifiers = ModifierFlags::None;

        void onKeyEvent(GtsKey key, bool pressed, int mods)
        {
            size_t idx = static_cast<size_t>(key);
            if (idx < KEY_COUNT)
                currentState[idx] = pressed;

            currentModifiers = ModifierFlags::None;
            if ((mods & 0x0001) != 0)
                currentModifiers |= ModifierFlags::Shift;
            if ((mods & 0x0002) != 0)
                currentModifiers |= ModifierFlags::Ctrl;
            if ((mods & 0x0004) != 0)
                currentModifiers |= ModifierFlags::Alt;
            if ((mods & 0x0008) != 0)
                currentModifiers |= ModifierFlags::Super;
        }

        // only the platform layer can signal the start of a new frame
        friend class GtsPlatform;
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

        ModifierFlags getModifiers() const override
        {
            return currentModifiers;
        }

        // reset the input manager completely
        void reset()
        {
            currentState.fill(false);
            previousState.fill(false);
            currentModifiers = ModifierFlags::None;
        }
};
