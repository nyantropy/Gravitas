#include "UiModalManager.h"

#include "UiDocument.h"
#include "UiNode.h"

UiModalId UiModalManager::pushModal(UiDocument& document, UiFocusManager& focusManager, const UiModalDesc& desc)
{
    if (desc.layer == UI_INVALID_LAYER || document.getLayerRoot(desc.layer) == UI_INVALID_HANDLE)
        return UI_INVALID_MODAL;

    UiModal modal;
    modal.id = nextModalId++;
    modal.desc = desc;
    modal.focusScope = focusManager.pushScope(desc.owner);

    modalStack.push_back(modal);

    UiHandle focusTarget = desc.initialFocus;
    if (focusTarget == UI_INVALID_HANDLE)
        focusTarget = desc.owner;
    if (focusTarget == UI_INVALID_HANDLE)
        focusTarget = document.getLayerRoot(desc.layer);

    focusManager.requestFocus(document, focusTarget, UiFocusReason::Programmatic);
    return modal.id;
}

bool UiModalManager::popModal(UiDocument& document,
                              UiFocusManager& focusManager,
                              UiModalId modalId,
                              UiModalDismissReason /*reason*/)
{
    if (modalStack.empty() || modalId == UI_INVALID_MODAL || modalStack.back().id != modalId)
        return false;

    UiModal modal = modalStack.back();
    modalStack.pop_back();
    return finishPop(document, focusManager, modal);
}

bool UiModalManager::dismissTopModal(UiDocument& document,
                                     UiFocusManager& focusManager,
                                     UiModalDismissReason reason)
{
    if (modalStack.empty())
        return false;

    return popModal(document, focusManager, modalStack.back().id, reason);
}

UiModalId UiModalManager::routeCancel(UiDocument& document, UiFocusManager& focusManager, bool& dismissed)
{
    dismissed = false;
    if (modalStack.empty())
        return UI_INVALID_MODAL;

    const UiModalId target = modalStack.back().id;
    if (modalStack.back().desc.dismissOnCancel)
        dismissed = popModal(document, focusManager, target, UiModalDismissReason::Cancel);

    return target;
}

void UiModalManager::clear()
{
    modalStack.clear();
    nextModalId = UI_INVALID_MODAL + 1;
}

void UiModalManager::clear(UiDocument& document, UiFocusManager& focusManager)
{
    while (!modalStack.empty())
    {
        UiModal modal = modalStack.back();
        modalStack.pop_back();
        finishPop(document, focusManager, modal);
    }

    clear();
}

void UiModalManager::pruneInvalidModals(UiDocument& document, UiFocusManager& focusManager)
{
    while (!modalStack.empty())
    {
        UiModalDismissReason reason = UiModalDismissReason::Programmatic;
        if (isModalAlive(document, modalStack.back(), reason))
            return;

        UiModal modal = modalStack.back();
        modalStack.pop_back();
        finishPop(document, focusManager, modal);
    }
}

bool UiModalManager::dismissModalsOwnedByMount(UiDocument& document,
                                               UiFocusManager& focusManager,
                                               UiMountId mountId)
{
    if (mountId == UI_INVALID_MOUNT)
        return false;

    bool dismissed = false;
    while (!modalStack.empty() && modalStack.back().desc.ownerMount == mountId)
    {
        dismissTopModal(document, focusManager, UiModalDismissReason::OwnerRemoved);
        dismissed = true;
    }

    return dismissed;
}

bool UiModalManager::dismissModalsForSubtree(UiDocument& document,
                                             UiFocusManager& focusManager,
                                             UiHandle root)
{
    if (root == UI_INVALID_HANDLE || document.findNode(root) == nullptr)
        return false;

    bool dismissed = false;
    while (!modalStack.empty())
    {
        const UiHandle owner = modalStack.back().desc.owner;
        if (owner == UI_INVALID_HANDLE || !document.isDescendantOf(owner, root))
            break;

        dismissTopModal(document, focusManager, UiModalDismissReason::OwnerRemoved);
        dismissed = true;
    }

    return dismissed;
}

UiModalId UiModalManager::topModalId() const
{
    return modalStack.empty() ? UI_INVALID_MODAL : modalStack.back().id;
}

UiLayerId UiModalManager::topLayer() const
{
    return modalStack.empty() ? UI_INVALID_LAYER : modalStack.back().desc.layer;
}

UiHandle UiModalManager::topOwner() const
{
    return modalStack.empty() ? UI_INVALID_HANDLE : modalStack.back().desc.owner;
}

const UiModal* UiModalManager::topModal() const
{
    return modalStack.empty() ? nullptr : &modalStack.back();
}

UiModalState UiModalManager::state() const
{
    UiModalState result;
    result.active = !modalStack.empty();
    result.depth = static_cast<uint32_t>(modalStack.size());
    if (!modalStack.empty())
    {
        const UiModal& modal = modalStack.back();
        result.modal = modal.id;
        result.owner = modal.desc.owner;
        result.ownerMount = modal.desc.ownerMount;
        result.layer = modal.desc.layer;
        result.pointerBlocked = modal.desc.consumePointer;
        result.keyboardBlocked = modal.desc.consumeKeyboard;
        result.navigationBlocked = modal.desc.consumeNavigation;
        result.textBlocked = modal.desc.consumeTextInput;
    }
    return result;
}

bool UiModalManager::blocksPointer() const
{
    return !modalStack.empty() && modalStack.back().desc.consumePointer;
}

bool UiModalManager::blocksKeyboard() const
{
    return !modalStack.empty() && modalStack.back().desc.consumeKeyboard;
}

bool UiModalManager::blocksNavigation() const
{
    return !modalStack.empty() && modalStack.back().desc.consumeNavigation;
}

bool UiModalManager::blocksTextInput() const
{
    return !modalStack.empty() && modalStack.back().desc.consumeTextInput;
}

bool UiModalManager::isLayerBlockedForPointer(const UiDocument& document, UiLayerId layerId) const
{
    if (modalStack.empty() || layerId == UI_INVALID_LAYER || !blocksPointer())
        return false;

    const UiModal& modal = modalStack.back();
    if (layerId == modal.desc.layer)
        return false;

    switch (modal.desc.layerBlocking)
    {
        case UiModalLayerBlocking::None:
            return false;

        case UiModalLayerBlocking::AllOtherLayers:
            return true;

        case UiModalLayerBlocking::LowerLayers:
        {
            int modalOrder = 0;
            int layerOrder = 0;
            if (!document.getLayerOrder(modal.desc.layer, modalOrder) ||
                !document.getLayerOrder(layerId, layerOrder))
            {
                return true;
            }
            return layerOrder <= modalOrder;
        }
    }

    return true;
}

bool UiModalManager::restrictsPointerToTopLayer() const
{
    return !modalStack.empty() &&
           blocksPointer() &&
           modalStack.back().desc.layerBlocking == UiModalLayerBlocking::AllOtherLayers;
}

bool UiModalManager::isModalAlive(const UiDocument& document,
                                  const UiModal& modal,
                                  UiModalDismissReason& reason) const
{
    const UiHandle layerRoot = document.getLayerRoot(modal.desc.layer);
    if (layerRoot == UI_INVALID_HANDLE)
    {
        reason = UiModalDismissReason::LayerRemoved;
        return false;
    }

    const UiNode* layerNode = document.findNode(layerRoot);
    if (layerNode == nullptr || !layerNode->state.visible || !layerNode->state.enabled)
    {
        reason = UiModalDismissReason::LayerRemoved;
        return false;
    }

    if (modal.desc.owner != UI_INVALID_HANDLE && !isNodeAvailable(document, modal.desc.owner))
    {
        reason = UiModalDismissReason::OwnerRemoved;
        return false;
    }

    return true;
}

bool UiModalManager::isNodeAvailable(const UiDocument& document, UiHandle handle) const
{
    const UiNode* node = document.findNode(handle);
    while (node != nullptr)
    {
        if (!node->state.visible || !node->state.enabled)
            return false;

        if (node->parent == UI_INVALID_HANDLE)
            return true;

        node = document.findNode(node->parent);
    }

    return false;
}

bool UiModalManager::finishPop(UiDocument& document, UiFocusManager& focusManager, UiModal modal)
{
    focusManager.popScope(document, modal.focusScope);
    if (!modal.desc.restoreFocus)
        focusManager.clearFocus(document, UiFocusReason::Clear);
    return true;
}
