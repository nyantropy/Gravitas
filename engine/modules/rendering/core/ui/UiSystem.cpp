#include "UiSystem.h"

#include <algorithm>
#include <chrono>
#include <utility>
#include <variant>

#include "GlyphLayoutEngine.h"

UiSystem::UiSystem(IResourceProvider* inResources)
    : resources(inResources)
{
    recreateDefaultSurface();
}

void UiSystem::clear()
{
    destroyAllCompositionRecords();
    surfaces.clear();
    orderedSurfaces.clear();
    commandCache.clear();
    emptyCommandBuffer.clear();
    lastEvents.clear();
    lastDispatchResult = {};
    lastMetrics = {};
    nextCompositionId = UI_INVALID_COMPOSITION + 1;
    nextSurfaceId = UI_DEFAULT_SURFACE + 1;
    recreateDefaultSurface();
}

void UiSystem::setEnabled(bool inEnabled)
{
    enabled = inEnabled;
    if (!enabled)
    {
        for (auto& [_, record] : surfaces)
            record.surface.clearInteractionState();
        lastEvents.clear();
        lastDispatchResult = {};
    }
}

bool UiSystem::isEnabled() const
{
    return enabled;
}

UiSurfaceId UiSystem::createSurface(const UiSurfaceDesc& desc)
{
    UiSurfaceDesc resolved = desc;
    const UiSurfaceId surfaceId = nextSurfaceId++;
    if (resolved.name.empty())
        resolved.name = "surface-" + std::to_string(surfaceId);

    SurfaceRecord record;
    record.surface.reset(surfaceId, resolved);
    surfaces.emplace(surfaceId, std::move(record));
    orderedSurfaces.push_back(surfaceId);
    sortSurfaces();
    commandCache.clear();
    return surfaceId;
}

bool UiSystem::destroySurface(UiSurfaceId surfaceId)
{
    if (surfaceId == UI_INVALID_SURFACE || surfaceId == defaultSurfaceId)
        return false;

    auto it = surfaces.find(surfaceId);
    if (it == surfaces.end())
        return false;

    destroyCompositionRecordsForSurface(surfaceId);
    it->second.surface.clear();
    surfaces.erase(it);
    orderedSurfaces.erase(std::remove(orderedSurfaces.begin(), orderedSurfaces.end(), surfaceId),
                          orderedSurfaces.end());
    if (lastDispatchResult.surface == surfaceId)
        lastDispatchResult = {};
    lastEvents.clear();
    commandCache.clear();
    return true;
}

bool UiSystem::setSurfaceDesc(UiSurfaceId surfaceId, const UiSurfaceDesc& desc)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    const int oldOrder = record->surface.order();
    const bool changed = record->surface.setDesc(desc);
    if (!changed)
        return true;

    record->commandCacheValid = false;
    commandCache.clear();
    if (oldOrder != record->surface.order())
        sortSurfaces();
    return true;
}

UiSurfaceId UiSystem::getDefaultSurface() const
{
    return defaultSurfaceId;
}

UiSurface* UiSystem::findSurface(UiSurfaceId surfaceId)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : &record->surface;
}

const UiSurface* UiSystem::findSurface(UiSurfaceId surfaceId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : &record->surface;
}

std::vector<UiSurfaceId> UiSystem::surfaceIds() const
{
    return orderedSurfaces;
}

UiHandle UiSystem::getRoot() const
{
    return getRoot(defaultSurfaceId);
}

UiHandle UiSystem::getRoot(UiSurfaceId surfaceId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? UI_INVALID_HANDLE : record->surface.document().getRoot();
}

UiLayerId UiSystem::createLayer(const std::string& name, int order)
{
    return createLayer(defaultSurfaceId, name, order);
}

UiLayerId UiSystem::createLayer(UiSurfaceId surfaceId, const std::string& name, int order)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return UI_INVALID_LAYER;

    record->commandCacheValid = false;
    commandCache.clear();
    return record->surface.document().createLayer(name, order);
}

bool UiSystem::removeLayer(UiLayerId layerId)
{
    return removeLayer(defaultSurfaceId, layerId);
}

bool UiSystem::removeLayer(UiSurfaceId surfaceId, UiLayerId layerId)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr || layerId == UI_INVALID_LAYER || layerId == UI_DEFAULT_LAYER)
        return false;

    UiDocument& document = record->surface.document();
    UiFocusManager& focusState = record->surface.focusManager();
    UiModalManager& modalState = record->surface.modalManager();
    UiMountManager& mountState = record->surface.mountManager();

    const UiHandle root = document.getLayerRoot(layerId);
    if (root == UI_INVALID_HANDLE)
        return false;

    const std::vector<UiMountId> layerMounts = mountState.mountsInLayer(layerId);
    for (UiMountId mount : layerMounts)
        destroyMount(surfaceId, mount);
    record->surface.dragDropManager().unregisterSubtree(document, root);
    record->surface.navigationGraph().unregisterSubtree(document, root);
    removeTextBindingsRecursive(*record, root);
    const bool removed = document.removeLayer(layerId);
    if (!removed)
        return false;

    record->commandCacheValid = false;
    commandCache.clear();
    mountState.pruneInvalidMounts(document);
    record->surface.dragDropManager().pruneInvalidNodes(document, focusState);
    modalState.pruneInvalidModals(document, focusState);
    focusState.pruneInvalidHandles(document);
    record->surface.inputDispatcher().pruneMissingHandles(document);
    lastEvents.clear();
    return true;
}

bool UiSystem::setLayerOrder(UiLayerId layerId, int order)
{
    return setLayerOrder(defaultSurfaceId, layerId, order);
}

bool UiSystem::setLayerOrder(UiSurfaceId surfaceId, UiLayerId layerId, int order)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    const bool changed = record->surface.document().setLayerOrder(layerId, order);
    if (changed)
    {
        record->commandCacheValid = false;
        commandCache.clear();
    }
    return changed;
}

bool UiSystem::setLayerState(UiLayerId layerId, const UiLayerState& state)
{
    return setLayerState(defaultSurfaceId, layerId, state);
}

bool UiSystem::setLayerState(UiSurfaceId surfaceId, UiLayerId layerId, const UiLayerState& state)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    UiDocument& document = record->surface.document();
    UiFocusManager& focusState = record->surface.focusManager();
    UiModalManager& modalState = record->surface.modalManager();
    const bool changed = document.setLayerState(layerId, state);
    if (changed)
    {
        record->surface.dragDropManager().pruneInvalidNodes(document, focusState);
        modalState.pruneInvalidModals(document, focusState);
        focusState.pruneInvalidHandles(document);
        record->commandCacheValid = false;
        commandCache.clear();
    }
    return changed;
}

UiHandle UiSystem::getLayerRoot(UiLayerId layerId) const
{
    return getLayerRoot(defaultSurfaceId, layerId);
}

UiHandle UiSystem::getLayerRoot(UiSurfaceId surfaceId, UiLayerId layerId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? UI_INVALID_HANDLE : record->surface.document().getLayerRoot(layerId);
}

UiLayerId UiSystem::getDefaultLayer() const
{
    return getDefaultLayer(defaultSurfaceId);
}

UiLayerId UiSystem::getDefaultLayer(UiSurfaceId surfaceId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? UI_INVALID_LAYER : record->surface.document().getDefaultLayer();
}

UiHandle UiSystem::createNode(UiNodeType type, UiHandle parent)
{
    return createNode(defaultSurfaceId, type, parent);
}

UiHandle UiSystem::createNode(UiSurfaceId surfaceId, UiNodeType type, UiHandle parent)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return UI_INVALID_HANDLE;

    record->commandCacheValid = false;
    commandCache.clear();
    return record->surface.document().createNode(type, parent);
}

bool UiSystem::removeNode(UiHandle handle)
{
    return removeNode(defaultSurfaceId, handle);
}

bool UiSystem::removeNode(UiSurfaceId surfaceId, UiHandle handle)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    UiDocument& document = record->surface.document();
    if (!document.canRemoveNode(handle))
        return false;

    UiMountManager& mountState = record->surface.mountManager();
    const UiMountId exactMount = mountState.mountFromRoot(handle);
    if (exactMount != UI_INVALID_MOUNT && exactMount != UI_ROOT_MOUNT)
        return destroyMount(surfaceId, exactMount);

    record->surface.dragDropManager().unregisterSubtree(document, handle);
    record->surface.navigationGraph().unregisterSubtree(document, handle);
    removeTextBindingsRecursive(*record, handle);
    const bool removed = document.removeNode(handle);
    if (!removed)
        return false;

    UiFocusManager& focusState = record->surface.focusManager();
    UiModalManager& modalState = record->surface.modalManager();
    mountState.pruneInvalidMounts(document);
    record->surface.dragDropManager().pruneInvalidNodes(document, focusState);
    record->surface.inputDispatcher().pruneMissingHandles(document);
    modalState.pruneInvalidModals(document, focusState);
    focusState.pruneInvalidHandles(document);
    record->commandCacheValid = false;
    commandCache.clear();
    lastEvents.clear();
    return true;
}

bool UiSystem::reparentNode(UiHandle handle, UiHandle newParent)
{
    return reparentNode(defaultSurfaceId, handle, newParent);
}

bool UiSystem::reparentNode(UiSurfaceId surfaceId, UiHandle handle, UiHandle newParent)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    UiDocument& document = record->surface.document();
    const bool changed = document.reparentNode(handle, newParent);
    if (changed)
    {
        record->surface.modalManager().pruneInvalidModals(document, record->surface.focusManager());
        record->surface.focusManager().pruneInvalidHandles(document);
        record->commandCacheValid = false;
        commandCache.clear();
    }
    return changed;
}

UiNode* UiSystem::findNode(UiHandle handle)
{
    return findNode(defaultSurfaceId, handle);
}

const UiNode* UiSystem::findNode(UiHandle handle) const
{
    return findNode(defaultSurfaceId, handle);
}

UiNode* UiSystem::findNode(UiSurfaceId surfaceId, UiHandle handle)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : record->surface.document().findNode(handle);
}

const UiNode* UiSystem::findNode(UiSurfaceId surfaceId, UiHandle handle) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : record->surface.document().findNode(handle);
}

bool UiSystem::setLayout(UiHandle handle, const UiLayoutSpec& layout)
{
    return setLayout(defaultSurfaceId, handle, layout);
}

bool UiSystem::setLayout(UiSurfaceId surfaceId, UiHandle handle, const UiLayoutSpec& layout)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    const bool changed = record->surface.document().setLayout(handle, layout);
    if (changed)
    {
        record->commandCacheValid = false;
        commandCache.clear();
    }
    return changed;
}

bool UiSystem::setTheme(const UiTheme& theme)
{
    return setTheme(defaultSurfaceId, theme);
}

bool UiSystem::setTheme(UiSurfaceId surfaceId, const UiTheme& theme)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    if (record->surface.theme() == theme)
        return true;

    record->surface.setTheme(theme);
    record->surface.document().markAllDirty(UiDirtyFlags::Layout | UiDirtyFlags::Visual);
    record->commandCacheValid = false;
    commandCache.clear();
    return true;
}

const UiTheme& UiSystem::theme() const
{
    return defaultSurfaceRecord().surface.theme();
}

const UiTheme* UiSystem::theme(UiSurfaceId surfaceId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : &record->surface.theme();
}

bool UiSystem::setStyle(UiHandle handle, const UiStyle& style)
{
    return setStyle(defaultSurfaceId, handle, style);
}

bool UiSystem::setStyle(UiSurfaceId surfaceId, UiHandle handle, const UiStyle& style)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    const bool changed = record->surface.document().setStyle(handle, style);
    if (changed)
    {
        record->commandCacheValid = false;
        commandCache.clear();
    }
    return changed;
}

bool UiSystem::setStyleClass(UiHandle handle, const std::string& styleClass)
{
    return setStyleClass(defaultSurfaceId, handle, styleClass);
}

bool UiSystem::setStyleClass(UiSurfaceId surfaceId, UiHandle handle, const std::string& styleClass)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    UiNode* node = record->surface.document().findNode(handle);
    if (node == nullptr)
        return false;

    UiStyle style = node->style;
    style.styleClass = styleClass;
    const bool changed = record->surface.document().setStyle(handle, style);
    if (changed)
    {
        record->commandCacheValid = false;
        commandCache.clear();
    }
    return changed;
}

bool UiSystem::setState(UiHandle handle, const UiStateFlags& state)
{
    return setState(defaultSurfaceId, handle, state);
}

bool UiSystem::setState(UiSurfaceId surfaceId, UiHandle handle, const UiStateFlags& state)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    UiDocument& document = record->surface.document();
    const bool changed = document.setState(handle, state);
    if (changed)
    {
        record->surface.modalManager().pruneInvalidModals(document, record->surface.focusManager());
        record->surface.focusManager().pruneInvalidHandles(document);
        record->commandCacheValid = false;
        commandCache.clear();
    }
    return changed;
}

bool UiSystem::setPayload(UiHandle handle, const UiNodePayload& payload)
{
    return setPayload(defaultSurfaceId, handle, payload);
}

bool UiSystem::setPayload(UiSurfaceId surfaceId, UiHandle handle, const UiNodePayload& payload)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    const bool changed = record->surface.document().setPayload(handle, payload);
    if (changed)
    {
        record->commandCacheValid = false;
        commandCache.clear();
    }
    return changed;
}

bool UiSystem::setTextFont(UiHandle handle, BitmapFont* font)
{
    return setTextFont(defaultSurfaceId, handle, font);
}

bool UiSystem::setTextFont(UiSurfaceId surfaceId, UiHandle handle, BitmapFont* font)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    const UiNode* node = record->surface.document().findNode(handle);
    if (!node || node->type != UiNodeType::Text) return false;

    const auto it = record->textBindings.find(handle);
    if (it != record->textBindings.end() && it->second == font)
        return true;

    record->textBindings[handle] = font;
    ++record->textBindingRevision;
    record->commandCacheValid = false;
    commandCache.clear();
    return true;
}

UiDocument& UiSystem::getDocument()
{
    return defaultSurfaceRecord().surface.document();
}

const UiDocument& UiSystem::getDocument() const
{
    return defaultSurfaceRecord().surface.document();
}

UiDocument* UiSystem::findDocument(UiSurfaceId surfaceId)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : &record->surface.document();
}

const UiDocument* UiSystem::findDocument(UiSurfaceId surfaceId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : &record->surface.document();
}

UiFocusManager& UiSystem::focusManager()
{
    return defaultSurfaceRecord().surface.focusManager();
}

const UiFocusManager& UiSystem::focusManager() const
{
    return defaultSurfaceRecord().surface.focusManager();
}

UiModalManager& UiSystem::modalManager()
{
    return defaultSurfaceRecord().surface.modalManager();
}

const UiModalManager& UiSystem::modalManager() const
{
    return defaultSurfaceRecord().surface.modalManager();
}

UiNavigationGraph& UiSystem::navigationGraph()
{
    return defaultSurfaceRecord().surface.navigationGraph();
}

const UiNavigationGraph& UiSystem::navigationGraph() const
{
    return defaultSurfaceRecord().surface.navigationGraph();
}

UiNavigationGraph* UiSystem::navigationGraph(UiSurfaceId surfaceId)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : &record->surface.navigationGraph();
}

const UiNavigationGraph* UiSystem::navigationGraph(UiSurfaceId surfaceId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : &record->surface.navigationGraph();
}

bool UiSystem::registerNavigationNode(UiHandle handle, const UiNavigationNodeDesc& desc)
{
    return registerNavigationNode(defaultSurfaceId, handle, desc);
}

bool UiSystem::registerNavigationNode(UiSurfaceId surfaceId, UiHandle handle, const UiNavigationNodeDesc& desc)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr || record->surface.document().findNode(handle) == nullptr)
        return false;

    return record->surface.navigationGraph().registerNode(handle, desc);
}

bool UiSystem::unregisterNavigationNode(UiHandle handle)
{
    return unregisterNavigationNode(defaultSurfaceId, handle);
}

bool UiSystem::unregisterNavigationNode(UiSurfaceId surfaceId, UiHandle handle)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record != nullptr && record->surface.navigationGraph().unregisterNode(handle);
}

UiDragDropManager& UiSystem::dragDropManager()
{
    return defaultSurfaceRecord().surface.dragDropManager();
}

const UiDragDropManager& UiSystem::dragDropManager() const
{
    return defaultSurfaceRecord().surface.dragDropManager();
}

UiDragDropManager* UiSystem::dragDropManager(UiSurfaceId surfaceId)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : &record->surface.dragDropManager();
}

const UiDragDropManager* UiSystem::dragDropManager(UiSurfaceId surfaceId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : &record->surface.dragDropManager();
}

bool UiSystem::registerDragSource(UiHandle handle, const UiDragSourceDesc& desc)
{
    return registerDragSource(defaultSurfaceId, handle, desc);
}

bool UiSystem::registerDragSource(UiSurfaceId surfaceId, UiHandle handle, const UiDragSourceDesc& desc)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr || record->surface.document().findNode(handle) == nullptr)
        return false;

    return record->surface.dragDropManager().registerSource(handle, desc);
}

bool UiSystem::unregisterDragSource(UiHandle handle)
{
    return unregisterDragSource(defaultSurfaceId, handle);
}

bool UiSystem::unregisterDragSource(UiSurfaceId surfaceId, UiHandle handle)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record != nullptr && record->surface.dragDropManager().unregisterSource(handle);
}

bool UiSystem::registerDropTarget(UiHandle handle, const UiDropTargetDesc& desc)
{
    return registerDropTarget(defaultSurfaceId, handle, desc);
}

bool UiSystem::registerDropTarget(UiSurfaceId surfaceId, UiHandle handle, const UiDropTargetDesc& desc)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr || record->surface.document().findNode(handle) == nullptr)
        return false;

    return record->surface.dragDropManager().registerTarget(handle, desc);
}

bool UiSystem::unregisterDropTarget(UiHandle handle)
{
    return unregisterDropTarget(defaultSurfaceId, handle);
}

bool UiSystem::unregisterDropTarget(UiSurfaceId surfaceId, UiHandle handle)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record != nullptr && record->surface.dragDropManager().unregisterTarget(handle);
}

UiMountManager& UiSystem::mountManager()
{
    return defaultSurfaceRecord().surface.mountManager();
}

const UiMountManager& UiSystem::mountManager() const
{
    return defaultSurfaceRecord().surface.mountManager();
}

UiSystem::Metrics UiSystem::getLastMetrics() const
{
    return lastMetrics;
}

bool UiSystem::measureText(UiHandle handle, UiTextMeasurement& outMeasurement) const
{
    return measureText(defaultSurfaceId, handle, outMeasurement);
}

bool UiSystem::measureText(UiSurfaceId surfaceId, UiHandle handle, UiTextMeasurement& outMeasurement) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    const UiDocument& document = record->surface.document();
    const UiNode* node = document.findNode(handle);
    if (node == nullptr || node->type != UiNodeType::Text)
        return false;

    const auto fontIt = record->textBindings.find(handle);
    if (fontIt == record->textBindings.end() || fontIt->second == nullptr)
        return false;

    UiTextData data = std::get<UiTextData>(node->payload);
    UiComputedStyle computedStyle;
    if (record->surface.theme().resolveNodeStyle(node->style, node->state, computedStyle)
        && computedStyle.hasTypography)
    {
        if (!computedStyle.typography.fontAsset.empty())
            data.fontAsset = computedStyle.typography.fontAsset;
        data.scale = computedStyle.typography.scale;
    }

    const float maxWidth = data.wrapMode == UiTextWrapMode::Word
        ? node->computedLayout.bounds.width
        : 0.0f;
    outMeasurement = GlyphLayoutEngine::measureUiText(
        *fontIt->second,
        data.text,
        data.scale,
        maxWidth,
        data.wrapMode,
        data.maxLines);
    return true;
}

UiInteractionResult UiSystem::updateInteraction(const UiInputFrame& input)
{
    return dispatchInput(input).toInteractionResult();
}

UiInteractionResult UiSystem::getLastInteraction() const
{
    return lastDispatchResult.toInteractionResult();
}

const UiDispatchResult& UiSystem::dispatchInput(const UiInputFrame& input, uint64_t frameId)
{
    if (frameId != 0 && lastDispatchResult.dispatched && lastDispatchResult.frameId == frameId)
        return lastDispatchResult;

    lastEvents.clear();
    if (!enabled)
    {
        for (auto& [_, record] : surfaces)
            record.surface.clearInteractionState();

        lastDispatchResult = {};
        lastDispatchResult.dispatched = true;
        lastDispatchResult.frameId = frameId;
        lastDispatchResult.surface = defaultSurfaceId;
        lastDispatchResult.pointerX = input.pointerX;
        lastDispatchResult.pointerY = input.pointerY;
        lastDispatchResult.scrollX = input.scrollX;
        lastDispatchResult.scrollY = input.scrollY;
        lastDispatchResult.primaryDown = input.primaryDown;
        lastDispatchResult.primaryPressed = input.primaryPressed;
        lastDispatchResult.primaryReleased = input.primaryReleased;
        lastDispatchResult.cancelPressed = input.cancelPressed;
        return lastDispatchResult;
    }

    const UiSurfaceId surfaceId = selectInputSurface(input);
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
    {
        lastDispatchResult = {};
        lastDispatchResult.dispatched = true;
        lastDispatchResult.frameId = frameId;
        lastDispatchResult.surface = UI_INVALID_SURFACE;
        lastDispatchResult.pointerX = input.pointerX;
        lastDispatchResult.pointerY = input.pointerY;
        return lastDispatchResult;
    }

    UiSurface& surface = record->surface;
    const UiInputFrame localInput = surface.toLocalInput(input);
    const UiDispatchResult& result =
        surface.inputDispatcher().dispatch(surface.document(),
                                           surface.focusManager(),
                                           surface.modalManager(),
                                           surface.navigationGraph(),
                                           surface.dragDropManager(),
                                           localInput,
                                           surface.isEnabled(),
                                           surface.id(),
                                           frameId);
    lastDispatchResult = result;
    propagateDispatchEvents(*record, result.dispatchSequence);
    return lastDispatchResult;
}

const UiDispatchResult& UiSystem::dispatchResult() const
{
    return lastDispatchResult;
}

const std::vector<UiEvent>& UiSystem::events() const
{
    return lastEvents;
}

bool UiSystem::propagateEvent(UiEvent event)
{
    if (event.surface == UI_INVALID_SURFACE)
        event.surface = defaultSurfaceId;

    const bool delivered = routeEvent(event);
    lastEvents.push_back(event);
    return delivered;
}

UiModalId UiSystem::pushModal(const UiModalDesc& desc)
{
    return pushModal(defaultSurfaceId, desc);
}

UiModalId UiSystem::pushModal(UiSurfaceId surfaceId, const UiModalDesc& desc)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return UI_INVALID_MODAL;

    UiDocument& document = record->surface.document();
    UiMountManager& mountState = record->surface.mountManager();

    UiModalDesc resolved = desc;
    if (resolved.ownerMount == UI_INVALID_MOUNT && resolved.owner != UI_INVALID_HANDLE)
        resolved.ownerMount = mountState.mountFromNode(document, resolved.owner);
    if (resolved.owner == UI_INVALID_HANDLE && resolved.ownerMount != UI_INVALID_MOUNT)
        resolved.owner = mountState.rootForMount(resolved.ownerMount);
    if (resolved.layer == UI_INVALID_LAYER && resolved.owner != UI_INVALID_HANDLE)
        resolved.layer = document.getNodeLayer(resolved.owner);
    if (resolved.layer == UI_INVALID_LAYER && resolved.ownerMount != UI_INVALID_MOUNT)
        resolved.layer = document.getNodeLayer(mountState.rootForMount(resolved.ownerMount));
    if (resolved.layer == UI_INVALID_LAYER)
        resolved.layer = document.getDefaultLayer();
    return record->surface.modalManager().pushModal(document, record->surface.focusManager(), resolved);
}

bool UiSystem::popModal(UiModalId modalId, UiModalDismissReason reason)
{
    return popModal(defaultSurfaceId, modalId, reason);
}

bool UiSystem::popModal(UiSurfaceId surfaceId, UiModalId modalId, UiModalDismissReason reason)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    return record->surface.modalManager().popModal(record->surface.document(),
                                                   record->surface.focusManager(),
                                                   modalId,
                                                   reason);
}

bool UiSystem::dismissTopModal(UiModalDismissReason reason)
{
    return dismissTopModal(defaultSurfaceId, reason);
}

bool UiSystem::dismissTopModal(UiSurfaceId surfaceId, UiModalDismissReason reason)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    return record->surface.modalManager().dismissTopModal(record->surface.document(),
                                                          record->surface.focusManager(),
                                                          reason);
}

UiMountId UiSystem::createMount(const UiMountDesc& desc)
{
    return createMount(defaultSurfaceId, desc);
}

UiMountId UiSystem::createMount(UiSurfaceId surfaceId, const UiMountDesc& desc)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return UI_INVALID_MOUNT;

    record->commandCacheValid = false;
    commandCache.clear();
    return record->surface.mountManager().createMount(record->surface.document(), desc);
}

bool UiSystem::destroyMount(UiMountId mountId)
{
    return destroyMount(defaultSurfaceId, mountId);
}

bool UiSystem::destroyMount(UiSurfaceId surfaceId, UiMountId mountId)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    UiDocument& document = record->surface.document();
    UiMountManager& mountState = record->surface.mountManager();
    const UiHandle root = mountState.rootForMount(mountId);
    if (root == UI_INVALID_HANDLE || root == document.getDocumentRoot())
        return false;

    destroyCompositionRecordsForMount(surfaceId, mountId);
    record->surface.dragDropManager().unregisterSubtree(document, root);
    record->surface.navigationGraph().unregisterSubtree(document, root);
    removeTextBindingsRecursive(*record, root);
    const bool destroyed = mountState.destroyMount(document,
                                                   record->surface.focusManager(),
                                                   record->surface.modalManager(),
                                                   mountId);
    if (!destroyed)
        return false;

    record->commandCacheValid = false;
    commandCache.clear();
    record->surface.inputDispatcher().pruneMissingHandles(document);
    record->surface.dragDropManager().pruneInvalidNodes(document, record->surface.focusManager());
    record->surface.modalManager().pruneInvalidModals(document, record->surface.focusManager());
    record->surface.focusManager().pruneInvalidHandles(document);
    lastEvents.clear();
    return true;
}

bool UiSystem::attachMount(UiMountId mountId, const UiMountAttachment& attachment)
{
    return attachMount(defaultSurfaceId, mountId, attachment);
}

bool UiSystem::attachMount(UiSurfaceId surfaceId, UiMountId mountId, const UiMountAttachment& attachment)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    UiDocument& document = record->surface.document();
    const bool attached = record->surface.mountManager().attachMount(document, mountId, attachment);
    if (attached)
    {
        record->commandCacheValid = false;
        commandCache.clear();
        record->surface.modalManager().pruneInvalidModals(document, record->surface.focusManager());
        record->surface.focusManager().pruneInvalidHandles(document);
    }
    return attached;
}

bool UiSystem::detachMount(UiMountId mountId)
{
    return detachMount(defaultSurfaceId, mountId);
}

bool UiSystem::detachMount(UiSurfaceId surfaceId, UiMountId mountId)
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return false;

    return attachMount(surfaceId,
                       mountId,
                       UiMountAttachment{.layer = record->surface.document().getDefaultLayer(),
                                         .parentMount = UI_ROOT_MOUNT});
}

UiMount* UiSystem::findMount(UiMountId mountId)
{
    return findMount(defaultSurfaceId, mountId);
}

const UiMount* UiSystem::findMount(UiMountId mountId) const
{
    return findMount(defaultSurfaceId, mountId);
}

UiMount* UiSystem::findMount(UiSurfaceId surfaceId, UiMountId mountId)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : record->surface.mountManager().findMount(mountId);
}

const UiMount* UiSystem::findMount(UiSurfaceId surfaceId, UiMountId mountId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? nullptr : record->surface.mountManager().findMount(mountId);
}

UiMountId UiSystem::mountFromNode(UiHandle handle) const
{
    return mountFromNode(defaultSurfaceId, handle);
}

UiMountId UiSystem::mountFromNode(UiSurfaceId surfaceId, UiHandle handle) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? UI_INVALID_MOUNT : record->surface.mountManager().mountFromNode(
        record->surface.document(),
        handle);
}

UiMountId UiSystem::rootMount() const
{
    return rootMount(defaultSurfaceId);
}

UiMountId UiSystem::rootMount(UiSurfaceId surfaceId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? UI_INVALID_MOUNT : record->surface.mountManager().rootMount();
}

UiHandle UiSystem::mountRoot(UiMountId mountId) const
{
    return mountRoot(defaultSurfaceId, mountId);
}

UiHandle UiSystem::mountRoot(UiSurfaceId surfaceId, UiMountId mountId) const
{
    const SurfaceRecord* record = findSurfaceRecord(surfaceId);
    return record == nullptr ? UI_INVALID_HANDLE : record->surface.mountManager().rootForMount(mountId);
}

UiCompositionId UiSystem::mountComposition(std::unique_ptr<UiComposition> composition,
                                           const UiMountDesc& desc)
{
    return mountComposition(defaultSurfaceId, std::move(composition), desc);
}

UiCompositionId UiSystem::mountComposition(UiSurfaceId surfaceId,
                                           std::unique_ptr<UiComposition> composition,
                                           const UiMountDesc& desc)
{
    if (!composition)
        return UI_INVALID_COMPOSITION;

    const UiMountId mount = createMount(surfaceId, desc);
    if (mount == UI_INVALID_MOUNT)
        return UI_INVALID_COMPOSITION;

    const UiCompositionId compositionId = attachComposition(surfaceId, mount, std::move(composition));
    if (compositionId == UI_INVALID_COMPOSITION)
        destroyMount(surfaceId, mount);

    return compositionId;
}

UiCompositionId UiSystem::attachComposition(UiMountId mountId,
                                            std::unique_ptr<UiComposition> composition)
{
    return attachComposition(defaultSurfaceId, mountId, std::move(composition));
}

UiCompositionId UiSystem::attachComposition(UiSurfaceId surfaceId,
                                            UiMountId mountId,
                                            std::unique_ptr<UiComposition> composition)
{
    if (!composition || findMount(surfaceId, mountId) == nullptr)
        return UI_INVALID_COMPOSITION;

    const uint64_t key = mountCompositionKey(surfaceId, mountId);
    if (mountToComposition.find(key) != mountToComposition.end())
        return UI_INVALID_COMPOSITION;

    MountedComposition record;
    record.id = nextCompositionId++;
    record.surface = surfaceId;
    record.mount = mountId;
    record.composition = std::move(composition);

    const UiCompositionId id = record.id;
    compositions.emplace(id, std::move(record));
    mountToComposition[key] = id;

    UiCompositionContext context = makeCompositionContext(surfaceId, mountId);
    compositions.at(id).composition->build(context);
    if (SurfaceRecord* surface = findSurfaceRecord(surfaceId))
    {
        surface->commandCacheValid = false;
        commandCache.clear();
    }
    return id;
}

bool UiSystem::updateComposition(UiCompositionId compositionId)
{
    auto it = compositions.find(compositionId);
    if (it == compositions.end() || it->second.composition == nullptr)
        return false;

    UiCompositionContext context = makeCompositionContext(it->second.surface, it->second.mount);
    if (context.root == UI_INVALID_HANDLE)
        return false;

    it->second.composition->update(context);
    return true;
}

void UiSystem::updateCompositions()
{
    std::vector<UiCompositionId> ids;
    ids.reserve(compositions.size());
    for (const auto& [id, record] : compositions)
        ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    for (UiCompositionId id : ids)
        updateComposition(id);
}

bool UiSystem::rebuildComposition(UiCompositionId compositionId)
{
    auto it = compositions.find(compositionId);
    if (it == compositions.end() || it->second.composition == nullptr)
        return false;

    UiCompositionContext context = makeCompositionContext(it->second.surface, it->second.mount);
    if (context.root == UI_INVALID_HANDLE)
        return false;

    it->second.composition->destroy(context);
    it->second.composition->build(context);
    if (SurfaceRecord* surface = findSurfaceRecord(it->second.surface))
    {
        surface->commandCacheValid = false;
        commandCache.clear();
    }
    return true;
}

bool UiSystem::destroyComposition(UiCompositionId compositionId)
{
    auto it = compositions.find(compositionId);
    if (it == compositions.end())
        return false;

    const UiSurfaceId surfaceId = it->second.surface;
    const UiMountId mount = it->second.mount;
    std::unique_ptr<UiComposition> composition = std::move(it->second.composition);
    mountToComposition.erase(mountCompositionKey(surfaceId, mount));
    compositions.erase(it);

    if (composition != nullptr)
    {
        UiCompositionContext context = makeCompositionContext(surfaceId, mount);
        composition->destroy(context);
    }

    return destroyMount(surfaceId, mount);
}

UiComposition* UiSystem::findComposition(UiCompositionId compositionId)
{
    const auto it = compositions.find(compositionId);
    return it == compositions.end() ? nullptr : it->second.composition.get();
}

const UiComposition* UiSystem::findComposition(UiCompositionId compositionId) const
{
    const auto it = compositions.find(compositionId);
    return it == compositions.end() ? nullptr : it->second.composition.get();
}

UiCompositionId UiSystem::compositionFromMount(UiMountId mountId) const
{
    return compositionFromMount(defaultSurfaceId, mountId);
}

UiCompositionId UiSystem::compositionFromMount(UiSurfaceId surfaceId, UiMountId mountId) const
{
    const auto it = mountToComposition.find(mountCompositionKey(surfaceId, mountId));
    return it == mountToComposition.end() ? UI_INVALID_COMPOSITION : it->second;
}

UiMountId UiSystem::compositionMount(UiCompositionId compositionId) const
{
    const auto it = compositions.find(compositionId);
    return it == compositions.end() ? UI_INVALID_MOUNT : it->second.mount;
}

UiSurfaceId UiSystem::compositionSurface(UiCompositionId compositionId) const
{
    const auto it = compositions.find(compositionId);
    return it == compositions.end() ? UI_INVALID_SURFACE : it->second.surface;
}

UiCommandBuffer UiSystem::extractCommands(int viewportWidth, int viewportHeight)
{
    return extractCommandsRef(viewportWidth, viewportHeight);
}

const UiCommandBuffer& UiSystem::extractCommandsRef(int viewportWidth, int viewportHeight)
{
    commandCache.clear();
    lastMetrics = {};
    if (!enabled)
        return emptyCommandBuffer;

    for (UiSurfaceId surfaceId : orderedSurfaces)
    {
        SurfaceRecord* record = findSurfaceRecord(surfaceId);
        if (record == nullptr || !record->surface.participatesInRendering())
            continue;

        const UiCommandBuffer& surfaceBuffer = extractSurfaceCommandsRef(surfaceId,
                                                                         viewportWidth,
                                                                         viewportHeight);
        appendSurfaceCommands(commandCache, surfaceBuffer, record->surface.rect());

        lastMetrics.layoutMs += record->lastMetrics.layoutMs;
        lastMetrics.visualMs += record->lastMetrics.visualMs;
        lastMetrics.commandBuildMs += record->lastMetrics.commandBuildMs;
        lastMetrics.nodeCount += record->lastMetrics.nodeCount;
        lastMetrics.primitiveCount += record->lastMetrics.primitiveCount;
        lastMetrics.commandCacheHit += record->lastMetrics.commandCacheHit;
    }

    lastMetrics.commandCount = static_cast<uint32_t>(commandCache.commands.size());
    lastMetrics.vertexCount = static_cast<uint32_t>(commandCache.vertices.size());
    lastMetrics.indexCount = static_cast<uint32_t>(commandCache.indices.size());
    return commandCache;
}

const UiCommandBuffer& UiSystem::extractSurfaceCommandsRef(UiSurfaceId surfaceId,
                                                           int viewportWidth,
                                                           int viewportHeight)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return emptyCommandBuffer;

    UiSurface& surface = record->surface;
    UiDocument& document = surface.document();
    if (!enabled || !surface.participatesInRendering())
    {
        record->lastMetrics = {};
        record->lastMetrics.nodeCount = static_cast<uint32_t>(document.getNodeCount());
        return emptyCommandBuffer;
    }

    document.setViewportSize(1.0f, 1.0f);

    record->lastMetrics = {};
    record->lastMetrics.nodeCount = static_cast<uint32_t>(document.getNodeCount());

    if (hasFlag(document.getDirtyFlags(), UiDirtyFlags::Layout))
    {
        UiDocument::UiTextMeasureCallback textMeasure =
            [&record, &document](UiHandle handle,
                                 const UiTextData& data,
                                 UiTextMeasurement& outMeasurement)
        {
            const UiNode* node = document.findNode(handle);
            if (node == nullptr || node->type != UiNodeType::Text)
                return false;

            const auto fontIt = record->textBindings.find(handle);
            if (fontIt == record->textBindings.end() || fontIt->second == nullptr)
                return false;

            outMeasurement = GlyphLayoutEngine::measureUiText(
                *fontIt->second,
                data.text,
                data.scale,
                0.0f,
                data.wrapMode,
                data.maxLines);
            return true;
        };

        const auto start = std::chrono::steady_clock::now();
        document.updateLayout(1.0f, 1.0f, textMeasure, &surface.theme());
        const auto end = std::chrono::steady_clock::now();
        record->lastMetrics.layoutMs =
            std::chrono::duration<float, std::milli>(end - start).count();
    }

    if (hasFlag(document.getDirtyFlags(), UiDirtyFlags::Visual))
    {
        const auto start = std::chrono::steady_clock::now();
        document.rebuildVisualList(&surface.theme());
        const auto end = std::chrono::steady_clock::now();
        record->lastMetrics.visualMs =
            std::chrono::duration<float, std::milli>(end - start).count();
    }

    record->lastMetrics.primitiveCount =
        static_cast<uint32_t>(document.getVisualList().primitives.size());

    const uint64_t documentRevision = document.getVisualRevision();
    if (record->commandCacheValid
        && record->cachedDocumentRevision == documentRevision
        && record->cachedTextBindingRevision == record->textBindingRevision
        && record->cachedViewportWidth == viewportWidth
        && record->cachedViewportHeight == viewportHeight)
    {
        record->lastMetrics.commandCacheHit = 1;
        record->lastMetrics.commandCount = static_cast<uint32_t>(record->commandCache.commands.size());
        record->lastMetrics.vertexCount = static_cast<uint32_t>(record->commandCache.vertices.size());
        record->lastMetrics.indexCount = static_cast<uint32_t>(record->commandCache.indices.size());
        return record->commandCache;
    }

    const auto commandStart = std::chrono::steady_clock::now();
    resolver.buildCommandBuffer(record->commandCache,
                                document.getVisualList(),
                                resources,
                                record->textBindings,
                                viewportWidth,
                                viewportHeight);
    const auto commandEnd = std::chrono::steady_clock::now();
    record->lastMetrics.commandBuildMs =
        std::chrono::duration<float, std::milli>(commandEnd - commandStart).count();
    record->lastMetrics.commandCount = static_cast<uint32_t>(record->commandCache.commands.size());
    record->lastMetrics.vertexCount = static_cast<uint32_t>(record->commandCache.vertices.size());
    record->lastMetrics.indexCount = static_cast<uint32_t>(record->commandCache.indices.size());

    record->commandCacheValid = true;
    record->cachedDocumentRevision = documentRevision;
    record->cachedTextBindingRevision = record->textBindingRevision;
    record->cachedViewportWidth = viewportWidth;
    record->cachedViewportHeight = viewportHeight;

    return record->commandCache;
}

UiSystem::SurfaceRecord* UiSystem::findSurfaceRecord(UiSurfaceId surfaceId)
{
    const auto it = surfaces.find(surfaceId);
    return it == surfaces.end() ? nullptr : &it->second;
}

const UiSystem::SurfaceRecord* UiSystem::findSurfaceRecord(UiSurfaceId surfaceId) const
{
    const auto it = surfaces.find(surfaceId);
    return it == surfaces.end() ? nullptr : &it->second;
}

UiSystem::SurfaceRecord& UiSystem::defaultSurfaceRecord()
{
    return surfaces.at(defaultSurfaceId);
}

const UiSystem::SurfaceRecord& UiSystem::defaultSurfaceRecord() const
{
    return surfaces.at(defaultSurfaceId);
}

UiSurfaceId UiSystem::selectInputSurface(const UiInputFrame& input) const
{
    if (input.hasNavigationRequest())
    {
        for (auto it = orderedSurfaces.rbegin(); it != orderedSurfaces.rend(); ++it)
        {
            const SurfaceRecord* record = findSurfaceRecord(*it);
            if (record == nullptr || !record->surface.participatesInInput())
                continue;
            if (record->surface.hasKeyboardOwnership())
                return *it;
        }
    }

    for (auto it = orderedSurfaces.rbegin(); it != orderedSurfaces.rend(); ++it)
    {
        const SurfaceRecord* record = findSurfaceRecord(*it);
        if (record == nullptr || !record->surface.participatesInInput())
            continue;
        if (record->surface.hasPointerOwnership())
            return *it;
    }

    for (auto it = orderedSurfaces.rbegin(); it != orderedSurfaces.rend(); ++it)
    {
        const SurfaceRecord* record = findSurfaceRecord(*it);
        if (record == nullptr || !record->surface.participatesInInput())
            continue;
        if (record->surface.containsScreenPoint(input.pointerX, input.pointerY))
            return *it;
    }

    return surfaces.find(defaultSurfaceId) == surfaces.end() ? UI_INVALID_SURFACE : defaultSurfaceId;
}

void UiSystem::sortSurfaces()
{
    std::sort(orderedSurfaces.begin(),
              orderedSurfaces.end(),
              [this](UiSurfaceId lhs, UiSurfaceId rhs)
              {
                  const SurfaceRecord* lhsRecord = findSurfaceRecord(lhs);
                  const SurfaceRecord* rhsRecord = findSurfaceRecord(rhs);
                  const int lhsOrder = lhsRecord == nullptr ? 0 : lhsRecord->surface.order();
                  const int rhsOrder = rhsRecord == nullptr ? 0 : rhsRecord->surface.order();
                  if (lhsOrder != rhsOrder)
                      return lhsOrder < rhsOrder;
                  return lhs < rhs;
              });
}

void UiSystem::recreateDefaultSurface()
{
    UiSurfaceDesc desc;
    desc.name = "default-screen";
    desc.kind = UiSurfaceKind::Screen;
    desc.order = 0;
    desc.rect = {0.0f, 0.0f, 1.0f, 1.0f};

    SurfaceRecord record;
    record.surface.reset(defaultSurfaceId, desc);
    surfaces.emplace(defaultSurfaceId, std::move(record));
    orderedSurfaces.push_back(defaultSurfaceId);
    sortSurfaces();
}

uint64_t UiSystem::mountCompositionKey(UiSurfaceId surfaceId, UiMountId mountId)
{
    return (static_cast<uint64_t>(surfaceId) << 32u) | static_cast<uint64_t>(mountId);
}

void UiSystem::removeTextBindingsRecursive(SurfaceRecord& record, UiHandle handle)
{
    const UiNode* node = record.surface.document().findNode(handle);
    if (!node) return;

    const std::vector<UiHandle> children = node->children;
    for (UiHandle child : children)
        removeTextBindingsRecursive(record, child);

    const size_t erased = record.textBindings.erase(handle);
    if (erased > 0)
    {
        ++record.textBindingRevision;
        record.commandCacheValid = false;
        commandCache.clear();
    }
}

UiCompositionContext UiSystem::makeCompositionContext(UiSurfaceId surfaceId, UiMountId mountId)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
    {
        return UiCompositionContext{
            .ui = *this,
            .document = defaultSurfaceRecord().surface.document(),
            .resources = resources,
            .surface = UI_INVALID_SURFACE,
            .mount = UI_INVALID_MOUNT,
            .root = UI_INVALID_HANDLE
        };
    }

    return UiCompositionContext{
        .ui = *this,
        .document = record->surface.document(),
        .resources = resources,
        .surface = surfaceId,
        .mount = mountId,
        .root = record->surface.mountManager().rootForMount(mountId)
    };
}

void UiSystem::destroyCompositionRecordsForMount(UiSurfaceId surfaceId, UiMountId mountId)
{
    SurfaceRecord* record = findSurfaceRecord(surfaceId);
    if (record == nullptr)
        return;

    const UiMount* mount = record->surface.mountManager().findMount(mountId);
    if (mount == nullptr)
        return;

    const std::vector<UiMountId> children = mount->childMounts;
    for (UiMountId child : children)
        destroyCompositionRecordsForMount(surfaceId, child);

    const uint64_t key = mountCompositionKey(surfaceId, mountId);
    const auto ownerIt = mountToComposition.find(key);
    if (ownerIt == mountToComposition.end())
        return;

    const UiCompositionId compositionId = ownerIt->second;
    auto compositionIt = compositions.find(compositionId);
    std::unique_ptr<UiComposition> composition;
    if (compositionIt != compositions.end())
    {
        composition = std::move(compositionIt->second.composition);
        compositions.erase(compositionIt);
    }

    mountToComposition.erase(ownerIt);

    if (composition != nullptr)
    {
        UiCompositionContext context = makeCompositionContext(surfaceId, mountId);
        composition->destroy(context);
    }
}

void UiSystem::destroyCompositionRecordsForSurface(UiSurfaceId surfaceId)
{
    std::vector<UiCompositionId> ids;
    for (const auto& [id, record] : compositions)
    {
        if (record.surface == surfaceId)
            ids.push_back(id);
    }

    std::sort(ids.begin(), ids.end());
    for (UiCompositionId id : ids)
    {
        auto it = compositions.find(id);
        if (it == compositions.end())
            continue;

        const UiMountId mount = it->second.mount;
        std::unique_ptr<UiComposition> composition = std::move(it->second.composition);
        mountToComposition.erase(mountCompositionKey(surfaceId, mount));
        compositions.erase(it);

        if (composition != nullptr)
        {
            UiCompositionContext context = makeCompositionContext(surfaceId, mount);
            composition->destroy(context);
        }
    }
}

void UiSystem::destroyAllCompositionRecords()
{
    std::vector<MountedComposition> records;
    records.reserve(compositions.size());
    for (auto& [id, record] : compositions)
    {
        records.push_back(MountedComposition{
            record.id,
            record.surface,
            record.mount,
            std::move(record.composition)
        });
    }

    compositions.clear();
    mountToComposition.clear();

    for (MountedComposition& record : records)
    {
        if (record.composition == nullptr)
            continue;

        UiCompositionContext context = makeCompositionContext(record.surface, record.mount);
        record.composition->destroy(context);
    }
}

void UiSystem::propagateDispatchEvents(SurfaceRecord& record, uint64_t dispatchSequence)
{
    if (dispatchSequence == 0 || record.lastPropagatedDispatchSequence == dispatchSequence)
        return;

    record.lastPropagatedDispatchSequence = dispatchSequence;
    for (UiEvent event : record.surface.inputDispatcher().events())
        propagateEvent(event);
}

bool UiSystem::routeEvent(UiEvent& event)
{
    resolveEventTarget(event);
    if (event.target == UI_INVALID_HANDLE || event.targetPath.empty())
        return false;

    const SurfaceRecord* record = findSurfaceRecord(event.surface);
    if (record == nullptr)
        return false;

    const UiDocument& document = record->surface.document();
    bool delivered = false;
    if (event.targetPath.size() > 1)
    {
        event.phase = UiEventPhase::Capture;
        for (size_t i = 0; i + 1 < event.targetPath.size(); ++i)
        {
            assignCurrentEventTarget(event, event.targetPath[i]);
            delivered = deliverEventToCurrentTarget(event) || delivered;
            if (event.consumed || document.findNode(event.target) == nullptr)
                return delivered;
        }
    }

    event.phase = UiEventPhase::Target;
    assignCurrentEventTarget(event, event.target);
    delivered = deliverEventToCurrentTarget(event) || delivered;
    if (event.consumed || document.findNode(event.target) == nullptr)
        return delivered;

    event.phase = UiEventPhase::Bubble;
    for (auto it = event.targetPath.rbegin(); it != event.targetPath.rend(); ++it)
    {
        if (document.findNode(*it) == nullptr)
            return delivered;

        assignCurrentEventTarget(event, *it);
        delivered = deliverEventToCurrentTarget(event) || delivered;
        if (event.consumed || document.findNode(event.target) == nullptr)
            return delivered;
    }

    event.phase = UiEventPhase::None;
    event.currentTarget = UI_INVALID_HANDLE;
    event.currentMount = UI_INVALID_MOUNT;
    event.currentComposition = UI_INVALID_COMPOSITION;
    return delivered;
}

bool UiSystem::deliverEventToCurrentTarget(UiEvent& event)
{
    if (event.currentTarget == UI_INVALID_HANDLE ||
        event.currentComposition == UI_INVALID_COMPOSITION)
    {
        return false;
    }

    auto it = compositions.find(event.currentComposition);
    if (it == compositions.end() || it->second.composition == nullptr || it->second.surface != event.surface)
        return false;

    UiCompositionContext context = makeCompositionContext(event.surface, event.currentMount);
    it->second.composition->onEvent(context, event);
    return true;
}

void UiSystem::resolveEventTarget(UiEvent& event) const
{
    if (event.surface == UI_INVALID_SURFACE)
        event.surface = defaultSurfaceId;

    const SurfaceRecord* record = findSurfaceRecord(event.surface);
    if (record == nullptr)
    {
        event.target = UI_INVALID_HANDLE;
        event.targetPath.clear();
        event.layer = UI_INVALID_LAYER;
        event.targetMount = UI_INVALID_MOUNT;
        event.targetComposition = UI_INVALID_COMPOSITION;
        return;
    }

    const UiDocument& document = record->surface.document();
    if (event.target == UI_INVALID_HANDLE || document.findNode(event.target) == nullptr)
    {
        event.target = UI_INVALID_HANDLE;
        event.targetPath.clear();
        event.layer = UI_INVALID_LAYER;
        event.targetMount = UI_INVALID_MOUNT;
        event.targetComposition = UI_INVALID_COMPOSITION;
        return;
    }

    event.targetPath = document.pathFromRoot(event.target);
    event.layer = document.getNodeLayer(event.target);
    event.targetMount = record->surface.mountManager().mountFromNode(document, event.target);
    event.targetComposition = compositionFromMount(event.surface, event.targetMount);

    if (const UiNode* node = document.findNode(event.target))
    {
        event.localX = event.pointerX - node->computedLayout.bounds.x;
        event.localY = event.pointerY - node->computedLayout.bounds.y;
    }
}

void UiSystem::assignCurrentEventTarget(UiEvent& event, UiHandle currentTarget) const
{
    const SurfaceRecord* record = findSurfaceRecord(event.surface);
    if (record == nullptr)
    {
        event.currentTarget = UI_INVALID_HANDLE;
        event.currentMount = UI_INVALID_MOUNT;
        event.currentComposition = UI_INVALID_COMPOSITION;
        return;
    }

    event.currentTarget = currentTarget;
    event.currentMount = record->surface.mountManager().mountFromNode(record->surface.document(), currentTarget);
    event.currentComposition = compositionFromMount(event.surface, event.currentMount);
}

void UiSystem::appendSurfaceCommands(UiCommandBuffer& destination,
                                     const UiCommandBuffer& source,
                                     const UiRect& surfaceRect) const
{
    if (source.empty())
        return;

    const uint32_t vertexBase = static_cast<uint32_t>(destination.vertices.size());
    const uint32_t indexBase = static_cast<uint32_t>(destination.indices.size());

    destination.vertices.reserve(destination.vertices.size() + source.vertices.size());
    for (UiVertex vertex : source.vertices)
    {
        vertex.pos.x = surfaceRect.x + vertex.pos.x * surfaceRect.width;
        vertex.pos.y = surfaceRect.y + vertex.pos.y * surfaceRect.height;
        destination.vertices.push_back(vertex);
    }

    destination.indices.reserve(destination.indices.size() + source.indices.size());
    for (uint32_t index : source.indices)
        destination.indices.push_back(vertexBase + index);

    destination.commands.reserve(destination.commands.size() + source.commands.size());
    for (UiDrawCommand command : source.commands)
    {
        command.indexOffset += indexBase;
        destination.commands.push_back(command);
    }
}
