#pragma once

#include <cstdint>
#include <vector>

#include "UiEvent.h"
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
    const std::vector<UiEvent>& events() const { return generatedEvents; }

    void clear();
    void pruneMissingHandles(const UiDocument& document);

private:
    void clearInteractionState(UiDocument& document,
                               UiFocusManager& focusManager,
                               UiModalManager& modalManager);
    UiDispatchResult makeDisabledResult(const UiInputFrame& input, uint64_t frameId);
    void assignLayers(const UiDocument& document, UiDispatchResult& result) const;
    void assignModalState(const UiModalManager& modalManager, UiDispatchResult& result) const;
    void buildEvents(const UiDocument& document,
                     const UiDispatchResult& previous,
                     const UiDispatchResult& result,
                     UiHandle cancelTargetOwner);
    UiEvent makePointerEvent(const UiDocument& document,
                             UiEventType type,
                             UiHandle target,
                             const UiDispatchResult& result) const;
    UiEvent makeFocusEvent(const UiDocument& document,
                           UiEventType type,
                           UiHandle target,
                           const UiDispatchResult& result) const;

    UiDispatchResult lastDispatch;
    std::vector<UiEvent> generatedEvents;
    uint64_t dispatchSequence = 0;
};
