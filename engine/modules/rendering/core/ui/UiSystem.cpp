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
    lastInteraction = {};
    activeHandle = UI_INVALID_HANDLE;
    focusedHandle = UI_INVALID_HANDLE;
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

UiHandle UiSystem::createNode(UiNodeType type, UiHandle parent)
{
    return document.createNode(type, parent);
}

bool UiSystem::removeNode(UiHandle handle)
{
    removeTextBindingsRecursive(handle);
    const bool removed = document.removeNode(handle);
    if (!removed)
        return false;

    if (activeHandle != UI_INVALID_HANDLE && document.findNode(activeHandle) == nullptr)
        activeHandle = UI_INVALID_HANDLE;
    if (focusedHandle != UI_INVALID_HANDLE && document.findNode(focusedHandle) == nullptr)
        focusedHandle = UI_INVALID_HANDLE;
    if (lastInteraction.hovered != UI_INVALID_HANDLE && document.findNode(lastInteraction.hovered) == nullptr)
        lastInteraction.hovered = UI_INVALID_HANDLE;
    if (lastInteraction.pressed != UI_INVALID_HANDLE && document.findNode(lastInteraction.pressed) == nullptr)
        lastInteraction.pressed = UI_INVALID_HANDLE;
    if (lastInteraction.focused != UI_INVALID_HANDLE && document.findNode(lastInteraction.focused) == nullptr)
        lastInteraction.focused = UI_INVALID_HANDLE;
    if (lastInteraction.clicked != UI_INVALID_HANDLE && document.findNode(lastInteraction.clicked) == nullptr)
        lastInteraction.clicked = UI_INVALID_HANDLE;
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
    if (!enabled)
    {
        lastInteraction = {};
        activeHandle = UI_INVALID_HANDLE;
        focusedHandle = UI_INVALID_HANDLE;
        return lastInteraction;
    }

    document.setViewportSize(1.0f, 1.0f);
    if (hasFlag(document.getDirtyFlags(), UiDirtyFlags::Layout))
        document.updateLayout(1.0f, 1.0f);

    const UiHandle previousHovered = lastInteraction.hovered;
    const UiHandle previousPressed = lastInteraction.pressed;
    const UiHandle previousFocused = focusedHandle;

    const UiHandle hovered = document.hitTest(input.pointerX, input.pointerY);
    if (input.primaryPressed)
    {
        activeHandle = hovered;
        if (hovered != UI_INVALID_HANDLE)
            focusedHandle = hovered;
    }

    UiHandle clicked = UI_INVALID_HANDLE;
    if (input.primaryReleased)
    {
        if (activeHandle != UI_INVALID_HANDLE && activeHandle == hovered)
            clicked = activeHandle;
        activeHandle = UI_INVALID_HANDLE;
    }

    const UiHandle pressed = input.primaryDown ? activeHandle : UI_INVALID_HANDLE;

    applyInteractionState(previousHovered, false, previousHovered == focusedHandle, false);
    applyInteractionState(previousPressed, previousPressed == hovered, previousPressed == focusedHandle, false);
    applyInteractionState(previousFocused, previousFocused == hovered, previousFocused == focusedHandle, false);
    applyInteractionState(hovered, true, hovered == focusedHandle, hovered == pressed);
    applyInteractionState(pressed, pressed == hovered, pressed == focusedHandle, pressed != UI_INVALID_HANDLE);
    applyInteractionState(focusedHandle,
                          focusedHandle == hovered,
                          focusedHandle != UI_INVALID_HANDLE,
                          focusedHandle == pressed);

    lastInteraction = {
        hovered,
        focusedHandle,
        pressed,
        clicked,
        input.pointerX,
        input.pointerY,
        input.scrollX,
        input.scrollY
    };
    return lastInteraction;
}

UiInteractionResult UiSystem::getLastInteraction() const
{
    return lastInteraction;
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

void UiSystem::applyInteractionState(UiHandle handle,
                                     bool hovered,
                                     bool focused,
                                     bool pressed)
{
    if (handle == UI_INVALID_HANDLE)
        return;

    UiNode* node = document.findNode(handle);
    if (node == nullptr)
        return;

    UiStateFlags state = node->state;
    state.hovered = hovered;
    state.focused = focused;
    state.pressed = pressed;
    document.setState(handle, state);
}
