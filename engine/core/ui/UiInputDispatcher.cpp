#include "UiInputDispatcher.h"

#include "UiDocument.h"
#include "UiNode.h"
#include "UiTypes.h"

const UiDispatchResult& UiInputDispatcher::dispatch(UiDocument& document,
                                                    const UiInputFrame& input,
                                                    bool enabled,
                                                    uint64_t frameId)
{
    if (frameId != 0 && lastDispatch.dispatched && lastDispatch.frameId == frameId)
        return lastDispatch;

    if (!enabled)
    {
        clearInteractionState(document);
        lastDispatch = makeDisabledResult(input, frameId);
        return lastDispatch;
    }

    document.setViewportSize(1.0f, 1.0f);
    if (hasFlag(document.getDirtyFlags(), UiDirtyFlags::Layout))
        document.updateLayout(1.0f, 1.0f);

    const UiHandle previousHovered = lastDispatch.hovered;
    const UiHandle previousPressed = lastDispatch.pressed;
    const UiHandle previousFocused = focusedHandle;

    const UiHandle hovered = document.hitTest(input.pointerX, input.pointerY);
    if (input.primaryPressed)
    {
        activeHandle = hovered;
        if (hovered != UI_INVALID_HANDLE)
            focusedHandle = hovered;
    }

    UiHandle released = UI_INVALID_HANDLE;
    UiHandle clicked = UI_INVALID_HANDLE;
    if (input.primaryReleased)
    {
        released = activeHandle;
        if (activeHandle != UI_INVALID_HANDLE && activeHandle == hovered)
            clicked = activeHandle;
        activeHandle = UI_INVALID_HANDLE;
    }

    const UiHandle active = activeHandle;
    const UiHandle pressed = input.primaryDown ? activeHandle : UI_INVALID_HANDLE;

    applyInteractionState(document, previousHovered, false, previousHovered == focusedHandle, false);
    applyInteractionState(document, previousPressed, previousPressed == hovered, previousPressed == focusedHandle, false);
    applyInteractionState(document, previousFocused, previousFocused == hovered, previousFocused == focusedHandle, false);
    applyInteractionState(document, hovered, true, hovered == focusedHandle, hovered == pressed);
    applyInteractionState(document, pressed, pressed == hovered, pressed == focusedHandle, pressed != UI_INVALID_HANDLE);
    applyInteractionState(document,
                          focusedHandle,
                          focusedHandle == hovered,
                          focusedHandle != UI_INVALID_HANDLE,
                          focusedHandle == pressed);

    UiDispatchResult result;
    result.dispatched = true;
    result.frameId = frameId;
    result.dispatchSequence = ++dispatchSequence;
    result.surface = UI_DEFAULT_SURFACE;
    result.hovered = hovered;
    result.focused = focusedHandle;
    result.pressed = pressed;
    result.released = released;
    result.clicked = clicked;
    result.active = active;
    result.pointerX = input.pointerX;
    result.pointerY = input.pointerY;
    result.scrollX = input.scrollX;
    result.scrollY = input.scrollY;
    result.primaryDown = input.primaryDown;
    result.primaryPressed = input.primaryPressed;
    result.primaryReleased = input.primaryReleased;
    assignLayers(document, result);

    result.pointerConsumed =
        result.hovered != UI_INVALID_HANDLE
        || result.pressed != UI_INVALID_HANDLE
        || result.released != UI_INVALID_HANDLE
        || result.clicked != UI_INVALID_HANDLE
        || result.active != UI_INVALID_HANDLE;
    if ((result.scrollX != 0.0f || result.scrollY != 0.0f) && result.hovered != UI_INVALID_HANDLE)
        result.pointerConsumed = true;

    lastDispatch = result;
    return lastDispatch;
}

void UiInputDispatcher::clear()
{
    lastDispatch = {};
    activeHandle = UI_INVALID_HANDLE;
    focusedHandle = UI_INVALID_HANDLE;
    dispatchSequence = 0;
}

void UiInputDispatcher::pruneMissingHandles(const UiDocument& document)
{
    if (activeHandle != UI_INVALID_HANDLE && document.findNode(activeHandle) == nullptr)
        activeHandle = UI_INVALID_HANDLE;
    if (focusedHandle != UI_INVALID_HANDLE && document.findNode(focusedHandle) == nullptr)
        focusedHandle = UI_INVALID_HANDLE;

    if (lastDispatch.hovered != UI_INVALID_HANDLE && document.findNode(lastDispatch.hovered) == nullptr)
        lastDispatch.hovered = UI_INVALID_HANDLE;
    if (lastDispatch.focused != UI_INVALID_HANDLE && document.findNode(lastDispatch.focused) == nullptr)
        lastDispatch.focused = UI_INVALID_HANDLE;
    if (lastDispatch.pressed != UI_INVALID_HANDLE && document.findNode(lastDispatch.pressed) == nullptr)
        lastDispatch.pressed = UI_INVALID_HANDLE;
    if (lastDispatch.released != UI_INVALID_HANDLE && document.findNode(lastDispatch.released) == nullptr)
        lastDispatch.released = UI_INVALID_HANDLE;
    if (lastDispatch.clicked != UI_INVALID_HANDLE && document.findNode(lastDispatch.clicked) == nullptr)
        lastDispatch.clicked = UI_INVALID_HANDLE;
    if (lastDispatch.active != UI_INVALID_HANDLE && document.findNode(lastDispatch.active) == nullptr)
        lastDispatch.active = UI_INVALID_HANDLE;

    assignLayers(document, lastDispatch);
}

void UiInputDispatcher::clearInteractionState(UiDocument& document)
{
    applyInteractionState(document, lastDispatch.hovered, false, false, false);
    applyInteractionState(document, lastDispatch.pressed, false, false, false);
    applyInteractionState(document, focusedHandle, false, false, false);
    activeHandle = UI_INVALID_HANDLE;
    focusedHandle = UI_INVALID_HANDLE;
}

void UiInputDispatcher::applyInteractionState(UiDocument& document,
                                              UiHandle handle,
                                              bool hovered,
                                              bool focused,
                                              bool pressed)
{
    if (handle == UI_INVALID_HANDLE)
        return;

    UiNode* node = document.findNode(handle);
    if (node == nullptr)
        return;

    UiStateFlags state = node->state;
    state.hovered = hovered;
    state.focused = focused;
    state.pressed = pressed;
    document.setState(handle, state);
}

UiDispatchResult UiInputDispatcher::makeDisabledResult(const UiInputFrame& input, uint64_t frameId)
{
    UiDispatchResult result;
    result.dispatched = true;
    result.frameId = frameId;
    result.dispatchSequence = ++dispatchSequence;
    result.surface = UI_DEFAULT_SURFACE;
    result.pointerX = input.pointerX;
    result.pointerY = input.pointerY;
    result.scrollX = input.scrollX;
    result.scrollY = input.scrollY;
    result.primaryDown = input.primaryDown;
    result.primaryPressed = input.primaryPressed;
    result.primaryReleased = input.primaryReleased;
    return result;
}

void UiInputDispatcher::assignLayers(const UiDocument& document, UiDispatchResult& result) const
{
    result.hoveredLayer = document.getNodeLayer(result.hovered);
    result.focusedLayer = document.getNodeLayer(result.focused);
    result.pressedLayer = document.getNodeLayer(result.pressed);
    result.releasedLayer = document.getNodeLayer(result.released);
    result.clickedLayer = document.getNodeLayer(result.clicked);
    result.activeLayer = document.getNodeLayer(result.active);
}
