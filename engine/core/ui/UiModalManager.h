#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "UiAnimationTypes.h"
#include "UiDocument.h"
#include "UiFocusManager.h"
#include "UiInteraction.h"
#include "UiMountTypes.h"

enum class UiModalLayerBlocking : uint8_t
{
    None = 0,
    LowerLayers,
    AllOtherLayers
};

enum class UiModalDismissReason : uint8_t
{
    Programmatic = 0,
    Cancel,
    LayerRemoved,
    OwnerRemoved,
    UiCleared
};

struct UiModalDesc
{
    UiLayerId layer = UI_INVALID_LAYER;
    UiHandle owner = UI_INVALID_HANDLE;
    UiMountId ownerMount = UI_INVALID_MOUNT;
    UiHandle initialFocus = UI_INVALID_HANDLE;
    UiModalLayerBlocking layerBlocking = UiModalLayerBlocking::AllOtherLayers;
    bool dismissOnCancel = true;
    bool restoreFocus = true;
    bool consumePointer = true;
    bool consumeKeyboard = true;
    bool consumeNavigation = true;
    bool consumeTextInput = true;
    std::optional<UiAnimationTiming> openAnimation;
    std::optional<UiAnimationTiming> closeAnimation;
    float openOpacityFrom = 0.0f;
    float openOpacityTo = 1.0f;
    float closeOpacityTo = 0.0f;
};

struct UiModal
{
    UiModalId id = UI_INVALID_MODAL;
    UiModalDesc desc;
    UiFocusScopeId focusScope = UI_INVALID_FOCUS_SCOPE;
};

struct UiModalState
{
    bool active = false;
    uint32_t depth = 0;
    UiModalId modal = UI_INVALID_MODAL;
    UiHandle owner = UI_INVALID_HANDLE;
    UiMountId ownerMount = UI_INVALID_MOUNT;
    UiLayerId layer = UI_INVALID_LAYER;
    bool pointerBlocked = false;
    bool keyboardBlocked = false;
    bool navigationBlocked = false;
    bool textBlocked = false;
};

class UiModalManager
{
public:
    UiModalId pushModal(UiDocument& document, UiFocusManager& focusManager, const UiModalDesc& desc);
    bool popModal(UiDocument& document,
                  UiFocusManager& focusManager,
                  UiModalId modalId,
                  UiModalDismissReason reason = UiModalDismissReason::Programmatic);
    bool dismissTopModal(UiDocument& document,
                         UiFocusManager& focusManager,
                         UiModalDismissReason reason = UiModalDismissReason::Programmatic);
    UiModalId routeCancel(UiDocument& document, UiFocusManager& focusManager, bool& dismissed);

    void clear();
    void clear(UiDocument& document, UiFocusManager& focusManager);
    void pruneInvalidModals(UiDocument& document, UiFocusManager& focusManager);
    bool dismissModalsOwnedByMount(UiDocument& document, UiFocusManager& focusManager, UiMountId mountId);
    bool dismissModalsForSubtree(UiDocument& document, UiFocusManager& focusManager, UiHandle root);

    bool hasModal() const { return !modalStack.empty(); }
    uint32_t depth() const { return static_cast<uint32_t>(modalStack.size()); }
    UiModalId topModalId() const;
    UiLayerId topLayer() const;
    UiHandle topOwner() const;
    const UiModal* topModal() const;
    UiModalState state() const;

    bool blocksPointer() const;
    bool blocksKeyboard() const;
    bool blocksNavigation() const;
    bool blocksTextInput() const;
    bool isLayerBlockedForPointer(const UiDocument& document, UiLayerId layerId) const;
    bool restrictsPointerToTopLayer() const;

private:
    bool isModalAlive(const UiDocument& document, const UiModal& modal, UiModalDismissReason& reason) const;
    bool isNodeAvailable(const UiDocument& document, UiHandle handle) const;
    bool finishPop(UiDocument& document, UiFocusManager& focusManager, UiModal modal);

    std::vector<UiModal> modalStack;
    UiModalId nextModalId = UI_INVALID_MODAL + 1;
};
