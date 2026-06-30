#pragma once

#include <cstdint>

#include "UiHandle.h"
#include "UiLayer.h"

using UiSurfaceId = uint32_t;

static constexpr UiSurfaceId UI_INVALID_SURFACE = 0;
static constexpr UiSurfaceId UI_DEFAULT_SURFACE = 1;

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

struct UiDispatchResult
{
    bool        dispatched = false;
    uint64_t    frameId = 0;
    uint64_t    dispatchSequence = 0;
    UiSurfaceId surface = UI_DEFAULT_SURFACE;

    UiHandle hovered = UI_INVALID_HANDLE;
    UiHandle focused = UI_INVALID_HANDLE;
    UiHandle pressed = UI_INVALID_HANDLE;
    UiHandle released = UI_INVALID_HANDLE;
    UiHandle clicked = UI_INVALID_HANDLE;
    UiHandle active = UI_INVALID_HANDLE;

    UiLayerId hoveredLayer = UI_INVALID_LAYER;
    UiLayerId focusedLayer = UI_INVALID_LAYER;
    UiLayerId pressedLayer = UI_INVALID_LAYER;
    UiLayerId releasedLayer = UI_INVALID_LAYER;
    UiLayerId clickedLayer = UI_INVALID_LAYER;
    UiLayerId activeLayer = UI_INVALID_LAYER;

    float pointerX = 0.0f;
    float pointerY = 0.0f;
    float scrollX = 0.0f;
    float scrollY = 0.0f;
    bool primaryDown = false;
    bool primaryPressed = false;
    bool primaryReleased = false;

    bool pointerConsumed = false;
    bool keyboardConsumed = false;
    bool navigationConsumed = false;
    bool textInputConsumed = false;

    UiInteractionResult toInteractionResult() const
    {
        return UiInteractionResult{
            hovered,
            focused,
            pressed,
            clicked,
            pointerX,
            pointerY,
            scrollX,
            scrollY
        };
    }
};
