#pragma once

#include "UiHandle.h"

struct UiInputFrame
{
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    bool  primaryDown = false;
    bool  primaryPressed = false;
    bool  primaryReleased = false;
    float scrollX = 0.0f;
    float scrollY = 0.0f;
};

struct UiInteractionResult
{
    UiHandle hovered = UI_INVALID_HANDLE;
    UiHandle focused = UI_INVALID_HANDLE;
    UiHandle pressed = UI_INVALID_HANDLE;
    UiHandle clicked = UI_INVALID_HANDLE;
    float    pointerX = 0.0f;
    float    pointerY = 0.0f;
    float    scrollX = 0.0f;
    float    scrollY = 0.0f;
};

