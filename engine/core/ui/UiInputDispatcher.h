#pragma once

#include <cstdint>

#include "UiInteraction.h"

class UiDocument;

class UiInputDispatcher
{
public:
    const UiDispatchResult& dispatch(UiDocument& document,
                                     const UiInputFrame& input,
                                     bool enabled,
                                     uint64_t frameId = 0);

    const UiDispatchResult& dispatchResult() const { return lastDispatch; }
    UiInteractionResult     interactionResult() const { return lastDispatch.toInteractionResult(); }

    void clear();
    void pruneMissingHandles(const UiDocument& document);

private:
    void clearInteractionState(UiDocument& document);
    void applyInteractionState(UiDocument& document,
                               UiHandle handle,
                               bool hovered,
                               bool focused,
                               bool pressed);
    UiDispatchResult makeDisabledResult(const UiInputFrame& input, uint64_t frameId);
    void assignLayers(const UiDocument& document, UiDispatchResult& result) const;

    UiDispatchResult lastDispatch;
    UiHandle activeHandle = UI_INVALID_HANDLE;
    UiHandle focusedHandle = UI_INVALID_HANDLE;
    uint64_t dispatchSequence = 0;
};
