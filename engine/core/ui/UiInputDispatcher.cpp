#include "UiInputDispatcher.h"

#include "UiDocument.h"
#include "UiTypes.h"

const UiDispatchResult& UiInputDispatcher::dispatch(
    UiDocument& document,
    UiFocusManager& focusManager,
    UiModalManager& modalManager,
    const UiInputFrame& input,
    bool enabled,
    uint64_t frameId)
{
    if (frameId != 0 && lastDispatch.dispatched && lastDispatch.frameId == frameId)
        return lastDispatch;

    if (!enabled)
    {
        clearInteractionState(document, focusManager, modalManager);
        lastDispatch = makeDisabledResult(input, frameId);
        return lastDispatch;
    }

    modalManager.pruneInvalidModals(document, focusManager);
    focusManager.pruneInvalidHandles(document);

    document.setViewportSize(1.0f, 1.0f);
    if (hasFlag(document.getDirtyFlags(), UiDirtyFlags::Layout))
        document.updateLayout(1.0f, 1.0f);

    bool dismissedByCancel = false;
    UiModalId cancelTarget = UI_INVALID_MODAL;
    if (input.cancelPressed)
        cancelTarget = modalManager.routeCancel(document, focusManager, dismissedByCancel);

    modalManager.pruneInvalidModals(document, focusManager);

    UiHandle hovered = UI_INVALID_HANDLE;
    if (modalManager.restrictsPointerToTopLayer())
        hovered = document.hitTestLayer(modalManager.topLayer(), input.pointerX, input.pointerY);
    else
        hovered = document.hitTest(input.pointerX, input.pointerY);

    const UiLayerId hoveredLayer = document.getNodeLayer(hovered);
    if (modalManager.isLayerBlockedForPointer(document, hoveredLayer))
        hovered = UI_INVALID_HANDLE;
    focusManager.setHovered(document, UI_PRIMARY_POINTER, hovered);

    UiHandle captured = focusManager.capturedNode(UI_PRIMARY_POINTER);
    const UiLayerId capturedLayer = document.getNodeLayer(captured);
    if (modalManager.isLayerBlockedForPointer(document, capturedLayer))
    {
        focusManager.releasePointer(UI_PRIMARY_POINTER);
        captured = UI_INVALID_HANDLE;
    }

    const UiHandle activeBeforeInput = focusManager.activePointerNode(UI_PRIMARY_POINTER);
    const UiLayerId activeLayer = document.getNodeLayer(activeBeforeInput);
    if (modalManager.isLayerBlockedForPointer(document, activeLayer))
        focusManager.clearActivePointer(document, UI_PRIMARY_POINTER);

    const UiHandle pointerTarget = captured != UI_INVALID_HANDLE ? captured : hovered;

    if (input.primaryPressed)
    {
        focusManager.setActivePointer(document, UI_PRIMARY_POINTER, pointerTarget);
        if (pointerTarget != UI_INVALID_HANDLE)
            focusManager.requestFocus(document, pointerTarget, UiFocusReason::Pointer);
    }

    UiHandle released = UI_INVALID_HANDLE;
    UiHandle clicked  = UI_INVALID_HANDLE;
    if (input.primaryReleased)
    {
        released = focusManager.activePointerNode(UI_PRIMARY_POINTER);
        if (released != UI_INVALID_HANDLE && released == pointerTarget)
            clicked = released;
        focusManager.clearActivePointer(document, UI_PRIMARY_POINTER);
    }
    else if (!input.primaryDown && !input.primaryPressed)
    {
        focusManager.clearActivePointer(document, UI_PRIMARY_POINTER);
    }

    const UiHandle active  = focusManager.activePointerNode(UI_PRIMARY_POINTER);
    const UiHandle pressed = input.primaryDown ? active : UI_INVALID_HANDLE;

    UiDispatchResult result;
    result.dispatched       = true;
    result.frameId          = frameId;
    result.dispatchSequence = ++dispatchSequence;
    result.surface          = UI_DEFAULT_SURFACE;
    result.hovered          = focusManager.hoveredNode(UI_PRIMARY_POINTER);
    result.focused          = focusManager.focusedNode();
    result.pressed          = pressed;
    result.released         = released;
    result.clicked          = clicked;
    result.active           = active;
    result.captured         = focusManager.capturedNode(UI_PRIMARY_POINTER);
    result.pointerX         = input.pointerX;
    result.pointerY         = input.pointerY;
    result.scrollX          = input.scrollX;
    result.scrollY          = input.scrollY;
    result.primaryDown      = input.primaryDown;
    result.primaryPressed   = input.primaryPressed;
    result.primaryReleased  = input.primaryReleased;
    result.cancelPressed    = input.cancelPressed;
    result.cancelTargetModal = cancelTarget;
    result.dismissedModal = dismissedByCancel ? cancelTarget : UI_INVALID_MODAL;
    assignLayers(document, result);
    assignModalState(modalManager, result);

    result.pointerConsumed = result.hovered != UI_INVALID_HANDLE || result.pressed != UI_INVALID_HANDLE ||
                             result.released != UI_INVALID_HANDLE || result.clicked != UI_INVALID_HANDLE ||
                             result.active != UI_INVALID_HANDLE || result.captured != UI_INVALID_HANDLE ||
                             result.pointerBlocked;
    if ((result.scrollX != 0.0f || result.scrollY != 0.0f) && result.hovered != UI_INVALID_HANDLE)
        result.pointerConsumed = true;
    result.cancelConsumed = result.cancelTargetModal != UI_INVALID_MODAL;
    result.keyboardConsumed = focusManager.focusedNode() != UI_INVALID_HANDLE ||
                              result.keyboardBlocked ||
                              result.cancelConsumed;
    result.navigationConsumed =
        focusManager.navigationFocusedNode(UI_PRIMARY_INPUT_DEVICE) != UI_INVALID_HANDLE ||
        result.navigationBlocked;
    result.textInputConsumed = focusManager.textInputFocusedNode() != UI_INVALID_HANDLE ||
                               result.textBlocked;

    lastDispatch = result;
    return lastDispatch;
}

void UiInputDispatcher::clear()
{
    lastDispatch     = {};
    dispatchSequence = 0;
}

void UiInputDispatcher::pruneMissingHandles(const UiDocument& document)
{
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
    if (lastDispatch.captured != UI_INVALID_HANDLE && document.findNode(lastDispatch.captured) == nullptr)
        lastDispatch.captured = UI_INVALID_HANDLE;

    assignLayers(document, lastDispatch);
}

void UiInputDispatcher::clearInteractionState(UiDocument& document,
                                              UiFocusManager& focusManager,
                                              UiModalManager& modalManager)
{
    modalManager.clear(document, focusManager);
    focusManager.clear(document);
}

UiDispatchResult UiInputDispatcher::makeDisabledResult(const UiInputFrame& input, uint64_t frameId)
{
    UiDispatchResult result;
    result.dispatched       = true;
    result.frameId          = frameId;
    result.dispatchSequence = ++dispatchSequence;
    result.surface          = UI_DEFAULT_SURFACE;
    result.pointerX         = input.pointerX;
    result.pointerY         = input.pointerY;
    result.scrollX          = input.scrollX;
    result.scrollY          = input.scrollY;
    result.primaryDown      = input.primaryDown;
    result.primaryPressed   = input.primaryPressed;
    result.primaryReleased  = input.primaryReleased;
    result.cancelPressed    = input.cancelPressed;
    return result;
}

void UiInputDispatcher::assignLayers(const UiDocument& document, UiDispatchResult& result) const
{
    result.hoveredLayer  = document.getNodeLayer(result.hovered);
    result.focusedLayer  = document.getNodeLayer(result.focused);
    result.pressedLayer  = document.getNodeLayer(result.pressed);
    result.releasedLayer = document.getNodeLayer(result.released);
    result.clickedLayer  = document.getNodeLayer(result.clicked);
    result.activeLayer   = document.getNodeLayer(result.active);
    result.capturedLayer = document.getNodeLayer(result.captured);
}

void UiInputDispatcher::assignModalState(const UiModalManager& modalManager, UiDispatchResult& result) const
{
    const UiModalState modalState = modalManager.state();
    result.modalActive = modalState.active;
    result.modalDepth = modalState.depth;
    result.modal = modalState.modal;
    result.modalOwner = modalState.owner;
    result.modalLayer = modalState.layer;
    result.pointerBlocked = modalState.pointerBlocked;
    result.keyboardBlocked = modalState.keyboardBlocked;
    result.navigationBlocked = modalState.navigationBlocked;
    result.textBlocked = modalState.textBlocked;
}
