#pragma once

#include <cstdint>

#include "UiHandle.h"
#include "UiLayer.h"
#include "UiMountTypes.h"

using UiSurfaceId = uint32_t;
using UiInputDeviceId = uint32_t;
using UiPointerId = uint32_t;
using UiModalId = uint32_t;

static constexpr UiSurfaceId UI_INVALID_SURFACE = 0;
static constexpr UiSurfaceId UI_DEFAULT_SURFACE = 1;
static constexpr UiInputDeviceId UI_PRIMARY_INPUT_DEVICE = 1;
static constexpr UiPointerId UI_PRIMARY_POINTER = 1;
static constexpr UiModalId UI_INVALID_MODAL = 0;

enum class UiNavigationDirection : uint8_t
{
    None = 0,
    Up,
    Down,
    Left,
    Right,
    Next,
    Previous
};

struct UiInputFrame
{
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    bool  primaryDown = false;
    bool  primaryPressed = false;
    bool  primaryReleased = false;
    bool  cancelPressed = false;
    bool  navigationUpPressed = false;
    bool  navigationDownPressed = false;
    bool  navigationLeftPressed = false;
    bool  navigationRightPressed = false;
    bool  navigationNextPressed = false;
    bool  navigationPreviousPressed = false;
    bool  navigationSubmitPressed = false;
    float scrollX = 0.0f;
    float scrollY = 0.0f;

    bool hasNavigationMove() const
    {
        return navigationUpPressed ||
               navigationDownPressed ||
               navigationLeftPressed ||
               navigationRightPressed ||
               navigationNextPressed ||
               navigationPreviousPressed;
    }

    bool hasNavigationRequest() const
    {
        return hasNavigationMove() || navigationSubmitPressed || cancelPressed;
    }

    UiNavigationDirection navigationDirection() const
    {
        if (navigationNextPressed)
            return UiNavigationDirection::Next;
        if (navigationPreviousPressed)
            return UiNavigationDirection::Previous;
        if (navigationUpPressed)
            return UiNavigationDirection::Up;
        if (navigationDownPressed)
            return UiNavigationDirection::Down;
        if (navigationLeftPressed)
            return UiNavigationDirection::Left;
        if (navigationRightPressed)
            return UiNavigationDirection::Right;
        return UiNavigationDirection::None;
    }
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
    UiHandle captured = UI_INVALID_HANDLE;
    UiModalId cancelTargetModal = UI_INVALID_MODAL;
    UiHandle cancelTargetOwner = UI_INVALID_HANDLE;
    UiModalId dismissedModal = UI_INVALID_MODAL;

    UiLayerId hoveredLayer = UI_INVALID_LAYER;
    UiLayerId focusedLayer = UI_INVALID_LAYER;
    UiLayerId pressedLayer = UI_INVALID_LAYER;
    UiLayerId releasedLayer = UI_INVALID_LAYER;
    UiLayerId clickedLayer = UI_INVALID_LAYER;
    UiLayerId activeLayer = UI_INVALID_LAYER;
    UiLayerId capturedLayer = UI_INVALID_LAYER;

    float pointerX = 0.0f;
    float pointerY = 0.0f;
    float scrollX = 0.0f;
    float scrollY = 0.0f;
    bool primaryDown = false;
    bool primaryPressed = false;
    bool primaryReleased = false;
    bool cancelPressed = false;
    bool cancelConsumed = false;
    bool navigationMoveRequested = false;
    bool navigationSubmitPressed = false;
    bool navigationMoved = false;
    bool navigationSubmitted = false;
    bool navigationRequestBlocked = false;
    bool navigationWrapped = false;
    UiNavigationDirection navigationDirection = UiNavigationDirection::None;
    UiHandle navigationFrom = UI_INVALID_HANDLE;
    UiHandle navigationTo = UI_INVALID_HANDLE;
    UiHandle navigationSubmitTarget = UI_INVALID_HANDLE;

    bool pointerConsumed = false;
    bool keyboardConsumed = false;
    bool navigationConsumed = false;
    bool textInputConsumed = false;

    bool modalActive = false;
    uint32_t modalDepth = 0;
    UiModalId modal = UI_INVALID_MODAL;
    UiHandle modalOwner = UI_INVALID_HANDLE;
    UiMountId modalMount = UI_INVALID_MOUNT;
    UiLayerId modalLayer = UI_INVALID_LAYER;
    bool pointerBlocked = false;
    bool keyboardBlocked = false;
    bool navigationBlocked = false;
    bool textBlocked = false;

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
