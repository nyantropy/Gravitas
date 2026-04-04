#include "UiDocument.h"

#include <algorithm>

namespace
{
    UiRect intersectRect(const UiRect& a, const UiRect& b)
    {
        const float left   = std::max(a.x, b.x);
        const float top    = std::max(a.y, b.y);
        const float right  = std::min(a.x + a.width,  b.x + b.width);
        const float bottom = std::min(a.y + a.height, b.y + b.height);

        if (right <= left || bottom <= top)
            return {left, top, 0.0f, 0.0f};

        return {left, top, right - left, bottom - top};
    }

    UiRect insetRect(const UiRect& rect, const UiThickness& inset)
    {
        const float left   = rect.x + inset.left;
        const float top    = rect.y + inset.top;
        const float right  = rect.x + rect.width  - inset.right;
        const float bottom = rect.y + rect.height - inset.bottom;

        return {
            left,
            top,
            std::max(0.0f, right - left),
            std::max(0.0f, bottom - top)
        };
    }
}

UiDocument::UiDocument()
{
    clear();
}

void UiDocument::clear()
{
    nodes.clear();
    visualList.primitives.clear();
    nextHandle = 1;
    dirtyFlags = UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual;

    rootHandle = allocHandle();

    UiNode root;
    root.handle = rootHandle;
    root.type   = UiNodeType::Container;
    root.parent = UI_INVALID_HANDLE;
    root.state.visible = true;
    root.state.enabled = true;
    root.state.interactable = false;
    root.layout.positionMode = UiPositionMode::Absolute;
    root.layout.widthMode    = UiSizeMode::Fixed;
    root.layout.heightMode   = UiSizeMode::Fixed;
    root.payload             = UiContainerData{};
    nodes.emplace(rootHandle, root);
}

UiHandle UiDocument::createNode(UiNodeType type, UiHandle parent)
{
    const UiHandle effectiveParent = (parent == UI_INVALID_HANDLE) ? rootHandle : parent;
    if (findNode(effectiveParent) == nullptr) return UI_INVALID_HANDLE;

    UiNode node;
    node.handle  = allocHandle();
    node.type    = type;
    node.parent  = effectiveParent;
    node.payload = [&]() -> UiNodePayload
    {
        switch (type)
        {
            case UiNodeType::Container: return UiContainerData{};
            case UiNodeType::Rect:      return UiRectData{};
            case UiNodeType::Image:     return UiImageData{};
            case UiNodeType::Text:      return UiTextData{};
            case UiNodeType::Grid:      return UiGridData{};
        }

        return UiContainerData{};
    }();

    nodes.emplace(node.handle, node);
    appendChild(effectiveParent, node.handle);
    markDirty(node.handle, UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual);
    return node.handle;
}

bool UiDocument::removeNode(UiHandle handle)
{
    if (handle == UI_INVALID_HANDLE || handle == rootHandle || findNode(handle) == nullptr)
        return false;

    detachFromParent(handle);
    removeNodeRecursive(handle);
    dirtyFlags |= UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual;
    return true;
}

bool UiDocument::reparentNode(UiHandle handle, UiHandle newParent)
{
    if (handle == UI_INVALID_HANDLE || handle == rootHandle) return false;
    if (findNode(handle) == nullptr || findNode(newParent) == nullptr) return false;

    detachFromParent(handle);
    nodes[handle].parent = newParent;
    appendChild(newParent, handle);
    markDirty(handle, UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual);
    return true;
}

UiNode* UiDocument::findNode(UiHandle handle)
{
    auto it = nodes.find(handle);
    return it == nodes.end() ? nullptr : &it->second;
}

const UiNode* UiDocument::findNode(UiHandle handle) const
{
    auto it = nodes.find(handle);
    return it == nodes.end() ? nullptr : &it->second;
}

bool UiDocument::setLayout(UiHandle handle, const UiLayoutSpec& layout)
{
    UiNode* node = findNode(handle);
    if (!node) return false;
    if (node->layout == layout) return true;

    node->layout = layout;
    markDirty(handle, UiDirtyFlags::Layout | UiDirtyFlags::Visual);
    return true;
}

bool UiDocument::setStyle(UiHandle handle, const UiStyle& style)
{
    UiNode* node = findNode(handle);
    if (!node) return false;
    if (node->style == style) return true;

    node->style = style;
    markDirty(handle, UiDirtyFlags::Visual);
    return true;
}

bool UiDocument::setState(UiHandle handle, const UiStateFlags& state)
{
    UiNode* node = findNode(handle);
    if (!node) return false;
    if (node->state == state) return true;

    node->state = state;
    markDirty(handle, UiDirtyFlags::Visual);
    return true;
}

bool UiDocument::setPayload(UiHandle handle, const UiNodePayload& payload)
{
    UiNode* node = findNode(handle);
    if (!node) return false;
    if (node->payload == payload) return true;

    node->payload = payload;
    markDirty(handle, UiDirtyFlags::Visual);
    return true;
}

void UiDocument::markDirty(UiHandle /*handle*/, UiDirtyFlags flags)
{
    dirtyFlags |= flags;
}

void UiDocument::markSubtreeDirty(UiHandle handle, UiDirtyFlags flags)
{
    if (findNode(handle) == nullptr) return;
    dirtyFlags |= flags;
}

void UiDocument::markAllDirty(UiDirtyFlags flags)
{
    dirtyFlags |= flags;
}

void UiDocument::setViewportSize(float inViewportWidth, float inViewportHeight)
{
    if (viewportWidth == inViewportWidth && viewportHeight == inViewportHeight)
        return;

    viewportWidth  = inViewportWidth;
    viewportHeight = inViewportHeight;
    dirtyFlags |= UiDirtyFlags::Layout | UiDirtyFlags::Visual;
}

void UiDocument::updateLayout(float inViewportWidth, float inViewportHeight)
{
    viewportWidth  = inViewportWidth;
    viewportHeight = inViewportHeight;

    UiNode& root = nodes[rootHandle];
    root.computedLayout.bounds      = {0.0f, 0.0f, viewportWidth, viewportHeight};
    root.computedLayout.contentRect = root.computedLayout.bounds;
    root.computedLayout.clipRect    = root.computedLayout.bounds;

    for (UiHandle child : root.children)
        computeLayoutRecursive(child, &root.computedLayout);

    dirtyFlags = static_cast<UiDirtyFlags>(static_cast<uint8_t>(dirtyFlags)
        & ~static_cast<uint8_t>(UiDirtyFlags::Layout));
}

void UiDocument::rebuildVisualList()
{
    visualList.primitives.clear();
    const UiRect rootClip = nodes[rootHandle].computedLayout.clipRect;
    rebuildVisualRecursive(rootHandle, true, rootClip);

    dirtyFlags = static_cast<UiDirtyFlags>(static_cast<uint8_t>(dirtyFlags)
        & ~static_cast<uint8_t>(UiDirtyFlags::Visual));
}

UiHandle UiDocument::allocHandle()
{
    return nextHandle++;
}

void UiDocument::removeNodeRecursive(UiHandle handle)
{
    const auto it = nodes.find(handle);
    if (it == nodes.end()) return;

    const std::vector<UiHandle> children = it->second.children;
    for (UiHandle child : children)
        removeNodeRecursive(child);

    nodes.erase(handle);
}

void UiDocument::detachFromParent(UiHandle handle)
{
    UiNode* node = findNode(handle);
    if (!node || node->parent == UI_INVALID_HANDLE) return;

    UiNode* parent = findNode(node->parent);
    if (!parent) return;

    auto& children = parent->children;
    children.erase(std::remove(children.begin(), children.end(), handle), children.end());
}

void UiDocument::appendChild(UiHandle parent, UiHandle child)
{
    nodes[parent].children.push_back(child);
}

void UiDocument::computeLayoutRecursive(UiHandle handle, const UiComputedLayout* parentLayout)
{
    UiNode& node = nodes[handle];
    const UiRect& parentRect = parentLayout->contentRect;

    float x = parentRect.x;
    float y = parentRect.y;
    float width = node.layout.fixedWidth;
    float height = node.layout.fixedHeight;

    if (node.layout.positionMode == UiPositionMode::Anchored)
    {
        const float left = parentRect.x + parentRect.width  * node.layout.anchorMin.x + node.layout.offsetMin.x + node.layout.margin.left;
        const float top  = parentRect.y + parentRect.height * node.layout.anchorMin.y + node.layout.offsetMin.y + node.layout.margin.top;
        x = left;
        y = top;

        if (node.layout.widthMode == UiSizeMode::FromAnchors)
        {
            const float right = parentRect.x + parentRect.width * node.layout.anchorMax.x + node.layout.offsetMax.x - node.layout.margin.right;
            width = std::max(0.0f, right - left);
        }

        if (node.layout.heightMode == UiSizeMode::FromAnchors)
        {
            const float bottom = parentRect.y + parentRect.height * node.layout.anchorMax.y + node.layout.offsetMax.y - node.layout.margin.bottom;
            height = std::max(0.0f, bottom - top);
        }
    }
    else
    {
        x = parentRect.x + node.layout.offsetMin.x + node.layout.margin.left;
        y = parentRect.y + node.layout.offsetMin.y + node.layout.margin.top;
        width  = std::max(0.0f, node.layout.fixedWidth  - node.layout.margin.left - node.layout.margin.right);
        height = std::max(0.0f, node.layout.fixedHeight - node.layout.margin.top  - node.layout.margin.bottom);
    }

    node.computedLayout.bounds      = {x, y, width, height};
    node.computedLayout.contentRect = insetRect(node.computedLayout.bounds, node.layout.padding);
    node.computedLayout.clipRect    = (node.layout.clipMode == UiClipMode::ClipChildren)
        ? intersectRect(parentLayout->clipRect, node.computedLayout.bounds)
        : parentLayout->clipRect;

    for (UiHandle child : node.children)
        computeLayoutRecursive(child, &node.computedLayout);
}

void UiDocument::rebuildVisualRecursive(UiHandle handle, bool parentVisible, const UiRect& inheritedClip)
{
    const UiNode& node = nodes[handle];
    const bool visible = parentVisible && node.isVisible();
    if (!visible) return;

    const UiRect effectiveClip = (node.layout.clipMode == UiClipMode::ClipChildren)
        ? intersectRect(inheritedClip, node.computedLayout.bounds)
        : inheritedClip;

    switch (node.type)
    {
        case UiNodeType::Container:
            break;

        case UiNodeType::Rect:
        {
            const auto& data = std::get<UiRectData>(node.payload);
            visualList.primitives.push_back(UiRectPrimitive{
                node.handle,
                node.computedLayout.bounds,
                effectiveClip,
                data.color
            });
            break;
        }

        case UiNodeType::Image:
        {
            const auto& data = std::get<UiImageData>(node.payload);
            visualList.primitives.push_back(UiImagePrimitive{
                node.handle,
                node.computedLayout.bounds,
                effectiveClip,
                data.imageAsset,
                data.tint,
                data.imageAspect
            });
            break;
        }

        case UiNodeType::Text:
        {
            const auto& data = std::get<UiTextData>(node.payload);
            visualList.primitives.push_back(UiTextPrimitive{
                node.handle,
                node.computedLayout.bounds,
                effectiveClip,
                data.text,
                data.fontAsset,
                data.color,
                data.scale
            });
            break;
        }

        case UiNodeType::Grid:
        {
            const auto& data = std::get<UiGridData>(node.payload);
            if (data.columns <= 0 || data.rows <= 0) break;

            const float cellWidth = node.computedLayout.bounds.width / static_cast<float>(data.columns);
            const float cellHeight = node.computedLayout.bounds.height / static_cast<float>(data.rows);
            const float inset = std::max(0.0f, data.cellInset);

            for (int y = 0; y < data.rows; ++y)
            {
                for (int x = 0; x < data.columns; ++x)
                {
                    const UiGridCellData* cell = data.cellAt(x, y);
                    const UiColor color = (cell && cell->visible) ? cell->color : data.hiddenColor;

                    visualList.primitives.push_back(UiRectPrimitive{
                        node.handle,
                        {
                            node.computedLayout.bounds.x + x * cellWidth + inset,
                            node.computedLayout.bounds.y + y * cellHeight + inset,
                            std::max(0.0f, cellWidth - inset * 2.0f),
                            std::max(0.0f, cellHeight - inset * 2.0f)
                        },
                        effectiveClip,
                        color
                    });
                }
            }
            break;
        }
    }

    for (UiHandle child : node.children)
        rebuildVisualRecursive(child, visible, effectiveClip);
}
