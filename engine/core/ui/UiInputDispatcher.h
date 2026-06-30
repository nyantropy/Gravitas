#pragma once

#include <cstdint>

#include "UiFocusManager.h"
#include "UiInteraction.h"
#include "UiModalManager.h"

class UiInputDispatcher
{
public:
    const UiDispatchResult& dispatch(UiDocument& document,
                                     UiFocusManager& focusManager,
                                     UiModalManager& modalManager,
                                     const UiInputFrame& input,
                                     bool enabled,
                                     uint64_t frameId = 0);

    const UiDispatchResult& dispatchResult() const { return lastDispatch; }
    UiInteractionResult     interactionResult() const { return lastDispatch.toInteractionResult(); }

    void clear();
    void pruneMissingHandles(const UiDocument& document);

private:
    void clearInteractionState(UiDocument& document,
                               UiFocusManager& focusManager,
                               UiModalManager& modalManager);
    UiDispatchResult makeDisabledResult(const UiInputFrame& input, uint64_t frameId);
    void assignLayers(const UiDocument& document, UiDispatchResult& result) const;
    void assignModalState(const UiModalManager& modalManager, UiDispatchResult& result) const;

    UiDispatchResult lastDispatch;
    uint64_t dispatchSequence = 0;
};
