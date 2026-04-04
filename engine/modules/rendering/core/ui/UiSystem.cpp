#include "UiSystem.h"

#include <chrono>

UiSystem::UiSystem(IResourceProvider* inResources)
    : resources(inResources)
{
}

void UiSystem::clear()
{
    document.clear();
    textBindings.clear();
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
    return document.removeNode(handle);
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

    textBindings[handle] = font;
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

UiCommandBuffer UiSystem::extractCommands(int viewportWidth, int viewportHeight)
{
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

    const auto commandStart = std::chrono::steady_clock::now();
    UiCommandBuffer buffer = resolver.buildCommandBuffer(document.getVisualList(),
                                                         resources,
                                                         textBindings,
                                                         viewportWidth,
                                                         viewportHeight);
    const auto commandEnd = std::chrono::steady_clock::now();
    lastMetrics.commandBuildMs =
        std::chrono::duration<float, std::milli>(commandEnd - commandStart).count();
    lastMetrics.commandCount = static_cast<uint32_t>(buffer.commands.size());

    return buffer;
}

void UiSystem::removeTextBindingsRecursive(UiHandle handle)
{
    const UiNode* node = document.findNode(handle);
    if (!node) return;

    const std::vector<UiHandle> children = node->children;
    for (UiHandle child : children)
        removeTextBindingsRecursive(child);

    textBindings.erase(handle);
}
