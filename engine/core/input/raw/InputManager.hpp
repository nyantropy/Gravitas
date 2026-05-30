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
        static constexpr size_t MOUSE_BUTTON_COUNT = 8;
        std::array<bool, KEY_COUNT> currentState{};
        std::array<bool, KEY_COUNT> previousState{};
        std::array<bool, MOUSE_BUTTON_COUNT> currentMouseButtonState{};
        std::array<bool, MOUSE_BUTTON_COUNT> previousMouseButtonState{};
        double currentMouseX = 0.0;
        double currentMouseY = 0.0;
        double frameScrollX = 0.0;
        double frameScrollY = 0.0;
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

        void updateModifiers(int mods)
        {
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

        void onMouseButtonEvent(int button, bool pressed, int mods)
        {
            if (button >= 0 && static_cast<size_t>(button) < MOUSE_BUTTON_COUNT)
                currentMouseButtonState[static_cast<size_t>(button)] = pressed;

            updateModifiers(mods);
        }

        void onCursorPositionEvent(double x, double y)
        {
            currentMouseX = x;
            currentMouseY = y;
        }

        void onScrollEvent(double x, double y)
        {
            frameScrollX += x;
            frameScrollY += y;
        }

        // only the platform layer can signal the start of a new frame
        friend class GtsPlatform;
        void beginFrame()
        {
            previousState = currentState;
            previousMouseButtonState = currentMouseButtonState;
            frameScrollX = 0.0;
            frameScrollY = 0.0;
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

        bool isMouseButtonDown(int button) const override
        {
            return button >= 0
                && static_cast<size_t>(button) < MOUSE_BUTTON_COUNT
                && currentMouseButtonState[static_cast<size_t>(button)];
        }

        bool isMouseButtonPressed(int button) const override
        {
            return button >= 0
                && static_cast<size_t>(button) < MOUSE_BUTTON_COUNT
                && currentMouseButtonState[static_cast<size_t>(button)]
                && !previousMouseButtonState[static_cast<size_t>(button)];
        }

        bool isMouseButtonReleased(int button) const override
        {
            return button >= 0
                && static_cast<size_t>(button) < MOUSE_BUTTON_COUNT
                && !currentMouseButtonState[static_cast<size_t>(button)]
                && previousMouseButtonState[static_cast<size_t>(button)];
        }

        double mouseX() const override { return currentMouseX; }
        double mouseY() const override { return currentMouseY; }
        double scrollX() const override { return frameScrollX; }
        double scrollY() const override { return frameScrollY; }

        ModifierFlags getModifiers() const override
        {
            return currentModifiers;
        }

        // reset the input manager completely
        void reset()
        {
            currentState.fill(false);
            previousState.fill(false);
            currentMouseButtonState.fill(false);
            previousMouseButtonState.fill(false);
            currentMouseX = 0.0;
            currentMouseY = 0.0;
            frameScrollX = 0.0;
            frameScrollY = 0.0;
            currentModifiers = ModifierFlags::None;
        }
};
