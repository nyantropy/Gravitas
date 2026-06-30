#pragma once

#include <cstdint>

#include "UiFocusManager.h"
#include "UiInteraction.h"

class UiInputDispatcher
{
public:
    const UiDispatchResult& dispatch(UiDocument& document,
                                     UiFocusManager& focusManager,
                                     const UiInputFrame& input,
                                     bool enabled,
                                     uint64_t frameId = 0);

    const UiDispatchResult& dispatchResult() const { return lastDispatch; }
    UiInteractionResult     interactionResult() const { return lastDispatch.toInteractionResult(); }

    void clear();
    void pruneMissingHandles(const UiDocument& document);

private:
    void clearInteractionState(UiDocument& document, UiFocusManager& focusManager);
    UiDispatchResult makeDisabledResult(const UiInputFrame& input, uint64_t frameId);
    void assignLayers(const UiDocument& document, UiDispatchResult& result) const;

    UiDispatchResult lastDispatch;
    uint64_t dispatchSequence = 0;
};
