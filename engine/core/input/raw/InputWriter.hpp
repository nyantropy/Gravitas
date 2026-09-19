#pragma once

#include "InputManager.hpp"

// Borrowed raw-input access. The manager must outlive the writer.
class InputWriter
{
public:
    explicit InputWriter(InputManager& input) : input(input) {}

    void beginFrame()
    {
        input.beginFrame();
    }

    void onKeyEvent(GtsKey key, bool pressed, int mods)
    {
        input.onKeyEvent(key, pressed, mods);
    }

    void onMouseButtonEvent(int button, bool pressed, int mods)
    {
        input.onMouseButtonEvent(button, pressed, mods);
    }

    void onCursorPositionEvent(double x, double y)
    {
        input.onCursorPositionEvent(x, y);
    }

    void onScrollEvent(double x, double y)
    {
        input.onScrollEvent(x, y);
    }

private:
    InputManager& input;
};
