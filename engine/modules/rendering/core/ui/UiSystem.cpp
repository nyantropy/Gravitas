#include "UiSystem.h"

#include <chrono>
#include <variant>

#include "GlyphLayoutEngine.h"

UiSystem::UiSystem(IResourceProvider* inResources)
    : resources(inResources)
{
}

void UiSystem::clear()
{
    document.clear();
    textBindings.clear();
    ++textBindingRevision;
    commandCache.clear();
    commandCacheValid = false;
    inputDispatcher.clear();
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

    removeTextBindingsRecursive(root);
    const bool removed = document.removeLayer(layerId);
    if (!removed)
        return false;

    commandCacheValid = false;
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
    return document.setLayerState(layerId, state);
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

    removeTextBindingsRecursive(handle);
    const bool removed = document.removeNode(handle);
    if (!removed)
        return false;

    inputDispatcher.pruneMissingHandles(document);
    return true;
}

bool UiSystem::reparentNode(UiHandle handle, UiHandle newParent)
{
    return document.reparentNode(handle, newParent);
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
    return document.setState(handle, state);
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
    return inputDispatcher.dispatch(document, input, enabled, frameId);
}

const UiDispatchResult& UiSystem::dispatchResult() const
{
    return inputDispatcher.dispatchResult();
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
