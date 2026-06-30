#include "UiSystem.h"

#include <algorithm>
#include <chrono>
#include <variant>

#include "GlyphLayoutEngine.h"

UiSystem::UiSystem(IResourceProvider* inResources)
    : resources(inResources)
{
    mountState.reset(document);
}

void UiSystem::clear()
{
    destroyAllCompositionRecords();
    document.clear();
    textBindings.clear();
    ++textBindingRevision;
    commandCache.clear();
    commandCacheValid = false;
    mountState.reset(document);
    modalState.clear();
    focusState.clear();
    inputDispatcher.clear();
    nextCompositionId = UI_INVALID_COMPOSITION + 1;
}

void UiSystem::setEnabled(bool inEnabled)
{
    enabled = inEnabled;
}

bool UiSystem::isEnabled() const
{
    return enabled;
}

UiHandle UiSystem::getRoot() const
{
    return document.getRoot();
}

UiLayerId UiSystem::createLayer(const std::string& name, int order)
{
    commandCacheValid = false;
    return document.createLayer(name, order);
}

bool UiSystem::removeLayer(UiLayerId layerId)
{
    if (layerId == UI_INVALID_LAYER || layerId == UI_DEFAULT_LAYER)
        return false;

    const UiHandle root = document.getLayerRoot(layerId);
    if (root == UI_INVALID_HANDLE)
        return false;

    const std::vector<UiMountId> layerMounts = mountState.mountsInLayer(layerId);
    for (UiMountId mount : layerMounts)
        destroyMount(mount);
    removeTextBindingsRecursive(root);
    const bool removed = document.removeLayer(layerId);
    if (!removed)
        return false;

    commandCacheValid = false;
    mountState.pruneInvalidMounts(document);
    modalState.pruneInvalidModals(document, focusState);
    focusState.pruneInvalidHandles(document);
    inputDispatcher.pruneMissingHandles(document);
    return true;
}

bool UiSystem::setLayerOrder(UiLayerId layerId, int order)
{
    const bool changed = document.setLayerOrder(layerId, order);
    if (changed)
        commandCacheValid = false;
    return changed;
}

bool UiSystem::setLayerState(UiLayerId layerId, const UiLayerState& state)
{
    const bool changed = document.setLayerState(layerId, state);
    if (changed)
    {
        modalState.pruneInvalidModals(document, focusState);
        focusState.pruneInvalidHandles(document);
    }
    return changed;
}

UiHandle UiSystem::getLayerRoot(UiLayerId layerId) const
{
    return document.getLayerRoot(layerId);
}

UiLayerId UiSystem::getDefaultLayer() const
{
    return document.getDefaultLayer();
}

UiHandle UiSystem::createNode(UiNodeType type, UiHandle parent)
{
    return document.createNode(type, parent);
}

bool UiSystem::removeNode(UiHandle handle)
{
    if (!document.canRemoveNode(handle))
        return false;

    const UiMountId exactMount = mountState.mountFromRoot(handle);
    if (exactMount != UI_INVALID_MOUNT && exactMount != UI_ROOT_MOUNT)
        return destroyMount(exactMount);

    removeTextBindingsRecursive(handle);
    const bool removed = document.removeNode(handle);
    if (!removed)
        return false;

    mountState.pruneInvalidMounts(document);
    inputDispatcher.pruneMissingHandles(document);
    modalState.pruneInvalidModals(document, focusState);
    focusState.pruneInvalidHandles(document);
    return true;
}

bool UiSystem::reparentNode(UiHandle handle, UiHandle newParent)
{
    const bool changed = document.reparentNode(handle, newParent);
    if (changed)
    {
        modalState.pruneInvalidModals(document, focusState);
        focusState.pruneInvalidHandles(document);
    }
    return changed;
}

UiNode* UiSystem::findNode(UiHandle handle)
{
    return document.findNode(handle);
}

const UiNode* UiSystem::findNode(UiHandle handle) const
{
    return document.findNode(handle);
}

bool UiSystem::setLayout(UiHandle handle, const UiLayoutSpec& layout)
{
    return document.setLayout(handle, layout);
}

bool UiSystem::setStyle(UiHandle handle, const UiStyle& style)
{
    return document.setStyle(handle, style);
}

bool UiSystem::setState(UiHandle handle, const UiStateFlags& state)
{
    const bool changed = document.setState(handle, state);
    if (changed)
    {
        modalState.pruneInvalidModals(document, focusState);
        focusState.pruneInvalidHandles(document);
    }
    return changed;
}

bool UiSystem::setPayload(UiHandle handle, const UiNodePayload& payload)
{
    return document.setPayload(handle, payload);
}

bool UiSystem::setTextFont(UiHandle handle, BitmapFont* font)
{
    const UiNode* node = document.findNode(handle);
    if (!node || node->type != UiNodeType::Text) return false;

    const auto it = textBindings.find(handle);
    if (it != textBindings.end() && it->second == font)
        return true;

    textBindings[handle] = font;
    ++textBindingRevision;
    commandCacheValid = false;
    return true;
}

UiDocument& UiSystem::getDocument()
{
    return document;
}

const UiDocument& UiSystem::getDocument() const
{
    return document;
}

UiFocusManager& UiSystem::focusManager()
{
    return focusState;
}

const UiFocusManager& UiSystem::focusManager() const
{
    return focusState;
}

UiModalManager& UiSystem::modalManager()
{
    return modalState;
}

const UiModalManager& UiSystem::modalManager() const
{
    return modalState;
}

UiMountManager& UiSystem::mountManager()
{
    return mountState;
}

const UiMountManager& UiSystem::mountManager() const
{
    return mountState;
}

UiSystem::Metrics UiSystem::getLastMetrics() const
{
    return lastMetrics;
}

bool UiSystem::measureText(UiHandle handle, UiTextMeasurement& outMeasurement) const
{
    const UiNode* node = document.findNode(handle);
    if (node == nullptr || node->type != UiNodeType::Text)
        return false;

    const auto fontIt = textBindings.find(handle);
    if (fontIt == textBindings.end() || fontIt->second == nullptr)
        return false;

    const UiTextData& data = std::get<UiTextData>(node->payload);
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
    return inputDispatcher.interactionResult();
}

const UiDispatchResult& UiSystem::dispatchInput(const UiInputFrame& input, uint64_t frameId)
{
    return inputDispatcher.dispatch(document, focusState, modalState, input, enabled, frameId);
}

const UiDispatchResult& UiSystem::dispatchResult() const
{
    return inputDispatcher.dispatchResult();
}

UiModalId UiSystem::pushModal(const UiModalDesc& desc)
{
    UiModalDesc resolved = desc;
    if (resolved.ownerMount == UI_INVALID_MOUNT && resolved.owner != UI_INVALID_HANDLE)
        resolved.ownerMount = mountState.mountFromNode(document, resolved.owner);
    if (resolved.owner == UI_INVALID_HANDLE && resolved.ownerMount != UI_INVALID_MOUNT)
        resolved.owner = mountState.rootForMount(resolved.ownerMount);
    return modalState.pushModal(document, focusState, resolved);
}

bool UiSystem::popModal(UiModalId modalId, UiModalDismissReason reason)
{
    return modalState.popModal(document, focusState, modalId, reason);
}

bool UiSystem::dismissTopModal(UiModalDismissReason reason)
{
    return modalState.dismissTopModal(document, focusState, reason);
}

UiMountId UiSystem::createMount(const UiMountDesc& desc)
{
    commandCacheValid = false;
    return mountState.createMount(document, desc);
}

bool UiSystem::destroyMount(UiMountId mountId)
{
    const UiHandle root = mountState.rootForMount(mountId);
    if (root == UI_INVALID_HANDLE || root == document.getDocumentRoot())
        return false;

    destroyCompositionRecordsForMount(mountId);
    removeTextBindingsRecursive(root);
    const bool destroyed = mountState.destroyMount(document, focusState, modalState, mountId);
    if (!destroyed)
        return false;

    commandCacheValid = false;
    inputDispatcher.pruneMissingHandles(document);
    modalState.pruneInvalidModals(document, focusState);
    focusState.pruneInvalidHandles(document);
    return true;
}

bool UiSystem::attachMount(UiMountId mountId, const UiMountAttachment& attachment)
{
    const bool attached = mountState.attachMount(document, mountId, attachment);
    if (attached)
    {
        commandCacheValid = false;
        modalState.pruneInvalidModals(document, focusState);
        focusState.pruneInvalidHandles(document);
    }
    return attached;
}

bool UiSystem::detachMount(UiMountId mountId)
{
    return attachMount(mountId, UiMountAttachment{.layer = document.getDefaultLayer(), .parentMount = UI_ROOT_MOUNT});
}

UiMount* UiSystem::findMount(UiMountId mountId)
{
    return mountState.findMount(mountId);
}

const UiMount* UiSystem::findMount(UiMountId mountId) const
{
    return mountState.findMount(mountId);
}

UiMountId UiSystem::mountFromNode(UiHandle handle) const
{
    return mountState.mountFromNode(document, handle);
}

UiMountId UiSystem::rootMount() const
{
    return mountState.rootMount();
}

UiHandle UiSystem::mountRoot(UiMountId mountId) const
{
    return mountState.rootForMount(mountId);
}

UiCompositionId UiSystem::mountComposition(std::unique_ptr<UiComposition> composition,
                                           const UiMountDesc& desc)
{
    if (!composition)
        return UI_INVALID_COMPOSITION;

    const UiMountId mount = createMount(desc);
    if (mount == UI_INVALID_MOUNT)
        return UI_INVALID_COMPOSITION;

    const UiCompositionId compositionId = attachComposition(mount, std::move(composition));
    if (compositionId == UI_INVALID_COMPOSITION)
        destroyMount(mount);

    return compositionId;
}

UiCompositionId UiSystem::attachComposition(UiMountId mountId,
                                            std::unique_ptr<UiComposition> composition)
{
    if (!composition || findMount(mountId) == nullptr || mountToComposition.find(mountId) != mountToComposition.end())
        return UI_INVALID_COMPOSITION;

    MountedComposition record;
    record.id = nextCompositionId++;
    record.mount = mountId;
    record.composition = std::move(composition);

    const UiCompositionId id = record.id;
    compositions.emplace(id, std::move(record));
    mountToComposition[mountId] = id;

    UiCompositionContext context = makeCompositionContext(mountId);
    compositions.at(id).composition->build(context);
    commandCacheValid = false;
    return id;
}

bool UiSystem::updateComposition(UiCompositionId compositionId)
{
    auto it = compositions.find(compositionId);
    if (it == compositions.end() || it->second.composition == nullptr)
        return false;

    UiCompositionContext context = makeCompositionContext(it->second.mount);
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

    UiCompositionContext context = makeCompositionContext(it->second.mount);
    if (context.root == UI_INVALID_HANDLE)
        return false;

    it->second.composition->destroy(context);
    it->second.composition->build(context);
    commandCacheValid = false;
    return true;
}

bool UiSystem::destroyComposition(UiCompositionId compositionId)
{
    auto it = compositions.find(compositionId);
    if (it == compositions.end())
        return false;

    const UiMountId mount = it->second.mount;
    std::unique_ptr<UiComposition> composition = std::move(it->second.composition);
    mountToComposition.erase(mount);
    compositions.erase(it);

    if (composition != nullptr)
    {
        UiCompositionContext context = makeCompositionContext(mount);
        composition->destroy(context);
    }

    return destroyMount(mount);
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
    const auto it = mountToComposition.find(mountId);
    return it == mountToComposition.end() ? UI_INVALID_COMPOSITION : it->second;
}

UiMountId UiSystem::compositionMount(UiCompositionId compositionId) const
{
    const auto it = compositions.find(compositionId);
    return it == compositions.end() ? UI_INVALID_MOUNT : it->second.mount;
}

UiCommandBuffer UiSystem::extractCommands(int viewportWidth, int viewportHeight)
{
    return extractCommandsRef(viewportWidth, viewportHeight);
}

const UiCommandBuffer& UiSystem::extractCommandsRef(int viewportWidth, int viewportHeight)
{
    if (!enabled)
    {
        lastMetrics = {};
        lastMetrics.nodeCount = static_cast<uint32_t>(document.getNodeCount());
        return emptyCommandBuffer;
    }

    document.setViewportSize(1.0f, 1.0f);

    lastMetrics = {};
    lastMetrics.nodeCount = static_cast<uint32_t>(document.getNodeCount());

    if (hasFlag(document.getDirtyFlags(), UiDirtyFlags::Layout))
    {
        const auto start = std::chrono::steady_clock::now();
        document.updateLayout(1.0f, 1.0f);
        const auto end = std::chrono::steady_clock::now();
        lastMetrics.layoutMs =
            std::chrono::duration<float, std::milli>(end - start).count();
    }

    if (hasFlag(document.getDirtyFlags(), UiDirtyFlags::Visual))
    {
        const auto start = std::chrono::steady_clock::now();
        document.rebuildVisualList();
        const auto end = std::chrono::steady_clock::now();
        lastMetrics.visualMs =
            std::chrono::duration<float, std::milli>(end - start).count();
    }

    lastMetrics.primitiveCount =
        static_cast<uint32_t>(document.getVisualList().primitives.size());

    const uint64_t documentRevision = document.getVisualRevision();
    if (commandCacheValid
        && cachedDocumentRevision == documentRevision
        && cachedTextBindingRevision == textBindingRevision
        && cachedViewportWidth == viewportWidth
        && cachedViewportHeight == viewportHeight)
    {
        lastMetrics.commandCacheHit = 1;
        lastMetrics.commandCount = static_cast<uint32_t>(commandCache.commands.size());
        lastMetrics.vertexCount = static_cast<uint32_t>(commandCache.vertices.size());
        lastMetrics.indexCount = static_cast<uint32_t>(commandCache.indices.size());
        return commandCache;
    }

    const auto commandStart = std::chrono::steady_clock::now();
    resolver.buildCommandBuffer(commandCache,
                                document.getVisualList(),
                                resources,
                                textBindings,
                                viewportWidth,
                                viewportHeight);
    const auto commandEnd = std::chrono::steady_clock::now();
    lastMetrics.commandBuildMs =
        std::chrono::duration<float, std::milli>(commandEnd - commandStart).count();
    lastMetrics.commandCount = static_cast<uint32_t>(commandCache.commands.size());
    lastMetrics.vertexCount = static_cast<uint32_t>(commandCache.vertices.size());
    lastMetrics.indexCount = static_cast<uint32_t>(commandCache.indices.size());

    commandCacheValid = true;
    cachedDocumentRevision = documentRevision;
    cachedTextBindingRevision = textBindingRevision;
    cachedViewportWidth = viewportWidth;
    cachedViewportHeight = viewportHeight;

    return commandCache;
}

void UiSystem::removeTextBindingsRecursive(UiHandle handle)
{
    const UiNode* node = document.findNode(handle);
    if (!node) return;

    const std::vector<UiHandle> children = node->children;
    for (UiHandle child : children)
        removeTextBindingsRecursive(child);

    const size_t erased = textBindings.erase(handle);
    if (erased > 0)
    {
        ++textBindingRevision;
        commandCacheValid = false;
    }
}

UiCompositionContext UiSystem::makeCompositionContext(UiMountId mountId)
{
    return UiCompositionContext{
        .ui = *this,
        .document = document,
        .resources = resources,
        .mount = mountId,
        .root = mountState.rootForMount(mountId)
    };
}

void UiSystem::destroyCompositionRecordsForMount(UiMountId mountId)
{
    const UiMount* mount = mountState.findMount(mountId);
    if (mount == nullptr)
        return;

    const std::vector<UiMountId> children = mount->childMounts;
    for (UiMountId child : children)
        destroyCompositionRecordsForMount(child);

    const auto ownerIt = mountToComposition.find(mountId);
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
        UiCompositionContext context = makeCompositionContext(mountId);
        composition->destroy(context);
    }
}

void UiSystem::destroyAllCompositionRecords()
{
    std::vector<MountedComposition> records;
    records.reserve(compositions.size());
    for (auto& [id, record] : compositions)
        records.push_back(MountedComposition{record.id, record.mount, std::move(record.composition)});

    compositions.clear();
    mountToComposition.clear();

    for (MountedComposition& record : records)
    {
        if (record.composition == nullptr)
            continue;

        UiCompositionContext context = makeCompositionContext(record.mount);
        record.composition->destroy(context);
    }
}
