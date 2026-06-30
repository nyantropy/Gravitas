#include "UiDocument.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace
{
    bool containsPoint(const UiRect& rect, float x, float y)
    {
        return x >= rect.x
            && y >= rect.y
            && x <= rect.x + rect.width
            && y <= rect.y + rect.height;
    }

    bool containsCirclePoint(const UiRect& rect, float x, float y)
    {
        const float radius = std::min(rect.width, rect.height) * 0.5f;
        if (radius <= 0.0f)
            return false;

        const float centerX = rect.x + rect.width * 0.5f;
        const float centerY = rect.y + rect.height * 0.5f;
        const float dx = x - centerX;
        const float dy = y - centerY;
        return dx * dx + dy * dy <= radius * radius;
    }

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
    layers.clear();
    visualList.primitives.clear();
    nextHandle = 1;
    nextLayerId = UI_DEFAULT_LAYER + 1;
    dirtyFlags = UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual;
    ++visualRevision;

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
    nodes.emplace(rootHandle, std::move(root));

    UiLayer defaultLayer;
    defaultLayer.id = UI_DEFAULT_LAYER;
    defaultLayer.name = "default";
    defaultLayer.order = 0;
    createLayerRoot(defaultLayer);
    defaultLayerRoot = defaultLayer.root;
    layers.emplace(defaultLayer.id, std::move(defaultLayer));
    sortLayerRoots();
}

UiLayerId UiDocument::createLayer(const std::string& name, int order)
{
    UiLayer layer;
    layer.id = nextLayerId++;
    layer.name = name;
    layer.order = order;
    createLayerRoot(layer);

    const UiLayerId id = layer.id;
    layers.emplace(id, std::move(layer));
    sortLayerRoots();
    markDirty(rootHandle, UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual);
    return id;
}

bool UiDocument::removeLayer(UiLayerId layerId)
{
    if (layerId == UI_INVALID_LAYER || layerId == UI_DEFAULT_LAYER)
        return false;

    auto it = layers.find(layerId);
    if (it == layers.end())
        return false;

    const UiHandle layerRoot = it->second.root;
    detachFromParent(layerRoot);
    removeNodeRecursive(layerRoot);
    layers.erase(it);
    markDirty(rootHandle, UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual);
    return true;
}

bool UiDocument::setLayerOrder(UiLayerId layerId, int order)
{
    auto it = layers.find(layerId);
    if (it == layers.end())
        return false;
    if (it->second.order == order)
        return true;

    it->second.order = order;
    sortLayerRoots();
    markDirty(rootHandle, UiDirtyFlags::Structure | UiDirtyFlags::Visual);
    return true;
}

bool UiDocument::setLayerState(UiLayerId layerId, const UiLayerState& state)
{
    auto it = layers.find(layerId);
    if (it == layers.end())
        return false;
    if (it->second.state == state)
        return true;

    it->second.state = state;

    UiNode* root = findNode(it->second.root);
    if (root != nullptr)
    {
        UiStateFlags rootState = root->state;
        rootState.visible = state.visible;
        rootState.enabled = state.inputEnabled;
        rootState.interactable = false;
        if (!state.visible || !state.inputEnabled)
        {
            rootState.hovered = false;
            rootState.focused = false;
            rootState.pressed = false;
        }
        setState(root->handle, rootState);
    }

    markDirty(rootHandle, UiDirtyFlags::Visual);
    return true;
}

UiHandle UiDocument::getLayerRoot(UiLayerId layerId) const
{
    const auto it = layers.find(layerId);
    return it == layers.end() ? UI_INVALID_HANDLE : it->second.root;
}

bool UiDocument::canRemoveNode(UiHandle handle) const
{
    return handle != UI_INVALID_HANDLE
        && handle != rootHandle
        && !isLayerRoot(handle)
        && findNode(handle) != nullptr;
}

UiHandle UiDocument::createNode(UiNodeType type, UiHandle parent)
{
    const UiHandle effectiveParent = (parent == UI_INVALID_HANDLE) ? defaultLayerRoot : parent;
    if (effectiveParent == rootHandle)
        return UI_INVALID_HANDLE;
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
            case UiNodeType::NineSlice: return UiNineSliceData{};
            case UiNodeType::Text:      return UiTextData{};
            case UiNodeType::Grid:      return UiGridData{};
            case UiNodeType::Line:      return UiLineData{};
            case UiNodeType::Circle:    return UiCircleData{};
        }

        return UiContainerData{};
    }();

    const UiHandle handle = node.handle;
    nodes.emplace(handle, std::move(node));
    appendChild(effectiveParent, handle);
    markDirty(handle, UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual);
    return handle;
}

bool UiDocument::removeNode(UiHandle handle)
{
    if (!canRemoveNode(handle))
        return false;

    detachFromParent(handle);
    removeNodeRecursive(handle);
    markDirty(handle, UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual);
    return true;
}

bool UiDocument::reparentNode(UiHandle handle, UiHandle newParent)
{
    if (handle == UI_INVALID_HANDLE
        || handle == rootHandle
        || isLayerRoot(handle)
        || newParent == rootHandle)
    {
        return false;
    }
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
    if (hasFlag(flags, UiDirtyFlags::Structure)
        || hasFlag(flags, UiDirtyFlags::Layout)
        || hasFlag(flags, UiDirtyFlags::Visual))
    {
        ++visualRevision;
    }
    dirtyFlags |= flags;
}

void UiDocument::markSubtreeDirty(UiHandle handle, UiDirtyFlags flags)
{
    if (findNode(handle) == nullptr) return;
    markDirty(handle, flags);
}

void UiDocument::markAllDirty(UiDirtyFlags flags)
{
    markDirty(rootHandle, flags);
}

void UiDocument::setViewportSize(float inViewportWidth, float inViewportHeight)
{
    if (viewportWidth == inViewportWidth && viewportHeight == inViewportHeight)
        return;

    viewportWidth  = inViewportWidth;
    viewportHeight = inViewportHeight;
    markDirty(rootHandle, UiDirtyFlags::Layout | UiDirtyFlags::Visual);
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

UiHandle UiDocument::hitTest(float x, float y) const
{
    return hitTestRecursive(rootHandle, x, y, true);
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

void UiDocument::sortLayerRoots()
{
    UiNode* root = findNode(rootHandle);
    if (root == nullptr)
        return;

    root->children.clear();
    root->children.reserve(layers.size());

    std::vector<const UiLayer*> sortedLayers;
    sortedLayers.reserve(layers.size());
    for (const auto& [_, layer] : layers)
        sortedLayers.push_back(&layer);

    std::sort(sortedLayers.begin(),
              sortedLayers.end(),
              [](const UiLayer* lhs, const UiLayer* rhs)
              {
                  if (lhs->order != rhs->order)
                      return lhs->order < rhs->order;
                  return lhs->id < rhs->id;
              });

    for (const UiLayer* layer : sortedLayers)
    {
        if (findNode(layer->root) != nullptr)
            root->children.push_back(layer->root);
    }
}

void UiDocument::createLayerRoot(UiLayer& layer)
{
    UiNode node;
    node.handle = allocHandle();
    node.type = UiNodeType::Container;
    node.parent = rootHandle;
    node.payload = UiContainerData{};
    node.state.visible = layer.state.visible;
    node.state.enabled = layer.state.inputEnabled;
    node.state.interactable = false;
    node.layout.positionMode = UiPositionMode::Anchored;
    node.layout.widthMode = UiSizeMode::FromAnchors;
    node.layout.heightMode = UiSizeMode::FromAnchors;
    node.layout.anchorMin = {0.0f, 0.0f};
    node.layout.anchorMax = {1.0f, 1.0f};

    layer.root = node.handle;
    nodes.emplace(node.handle, std::move(node));
}

bool UiDocument::isLayerRoot(UiHandle handle) const
{
    for (const auto& [_, layer] : layers)
    {
        if (layer.root == handle)
            return true;
    }

    return false;
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

    UiComputedLayout childParentLayout = node.computedLayout;
    childParentLayout.contentRect.x += node.layout.contentOffset.x;
    childParentLayout.contentRect.y += node.layout.contentOffset.y;

    for (UiHandle child : node.children)
        computeLayoutRecursive(child, &childParentLayout);
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
                data.textureID,
                data.tint,
                data.imageAspect,
                data.rotation
            });
            break;
        }

        case UiNodeType::NineSlice:
        {
            const auto& data = std::get<UiNineSliceData>(node.payload);
            visualList.primitives.push_back(UiNineSlicePrimitive{
                node.handle,
                node.computedLayout.bounds,
                effectiveClip,
                data.imageAsset,
                data.tint,
                data.slice
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
                data.scale,
                data.wrapMode,
                data.horizontalAlign,
                data.verticalAlign,
                data.maxLines
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

        case UiNodeType::Line:
        {
            const auto& data = std::get<UiLineData>(node.payload);
            const UiRect& content = node.computedLayout.contentRect;
            const UiVec2 start = {
                content.x + data.start.x * content.width,
                content.y + data.start.y * content.height
            };
            const UiVec2 end = {
                content.x + data.end.x * content.width,
                content.y + data.end.y * content.height
            };
            visualList.primitives.push_back(UiLinePrimitive{
                node.handle,
                start,
                end,
                effectiveClip,
                data.color,
                data.thickness
            });
            break;
        }

        case UiNodeType::Circle:
        {
            const auto& data = std::get<UiCircleData>(node.payload);
            visualList.primitives.push_back(UiCirclePrimitive{
                node.handle,
                node.computedLayout.bounds,
                effectiveClip,
                data.color,
                data.segments
            });
            break;
        }
    }

    for (UiHandle child : node.children)
        rebuildVisualRecursive(child, visible, effectiveClip);
}

UiHandle UiDocument::hitTestRecursive(UiHandle handle, float x, float y, bool parentVisible) const
{
    const auto it = nodes.find(handle);
    if (it == nodes.end())
        return UI_INVALID_HANDLE;

    const UiNode& node = it->second;
    const bool visible = parentVisible && node.isVisible();
    if (!visible || !node.state.enabled)
        return UI_INVALID_HANDLE;

    if (node.layout.clipMode == UiClipMode::ClipChildren
        && !containsPoint(node.computedLayout.clipRect, x, y))
    {
        return UI_INVALID_HANDLE;
    }

    for (auto childIt = node.children.rbegin(); childIt != node.children.rend(); ++childIt)
    {
        const UiHandle hit = hitTestRecursive(*childIt, x, y, visible);
        if (hit != UI_INVALID_HANDLE)
            return hit;
    }

    const bool hit = node.type == UiNodeType::Circle
        ? containsCirclePoint(node.computedLayout.bounds, x, y)
        : containsPoint(node.computedLayout.bounds, x, y);
    if (node.state.interactable && hit)
        return handle;

    return UI_INVALID_HANDLE;
}
