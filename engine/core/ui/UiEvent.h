#pragma once

#include <cstdint>

#include "UiInteraction.h"

enum class UiEventType : uint8_t
{
    PointerMove = 0,
    PointerEnter,
    PointerLeave,
    PointerDown,
    PointerUp,
    PointerClick,
    PointerWheel,
    KeyDown,
    KeyUp,
    TextInput,
    NavigationMove,
    NavigationSubmit,
    NavigationCancel,
    FocusGained,
    FocusLost,
    DragStart,
    DragMove,
    DragDrop,
    DragCancel
};

enum class UiEventPhase : uint8_t
{
    None = 0,
    Capture,
    Target,
    Bubble
};

struct UiEvent
{
    UiEventType type = UiEventType::PointerMove;
    UiEventPhase phase = UiEventPhase::None;
    UiSurfaceId surface = UI_DEFAULT_SURFACE;
    UiLayerId layer = UI_INVALID_LAYER;
    UiHandle target = UI_INVALID_HANDLE;
    UiHandle currentTarget = UI_INVALID_HANDLE;
    UiInputDeviceId deviceId = 0;
    UiPointerId pointerId = 0;
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    float scrollX = 0.0f;
    float scrollY = 0.0f;
    int keyCode = 0;
    uint32_t modifiers = 0;
    char32_t textCodepoint = 0;
    bool consumed = false;
    bool defaultPrevented = false;
};
