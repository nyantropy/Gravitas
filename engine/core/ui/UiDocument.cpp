#include "UiDocument.h"

#include <algorithm>
#include <cmath>
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

    UiRect applyMargin(const UiRect& rect, const UiThickness& margin)
    {
        return insetRect(rect, margin);
    }

    UiVec2 makeSize(float width, float height)
    {
        return {std::max(0.0f, width), std::max(0.0f, height)};
    }

    bool lengthIsExplicit(const UiLayoutLength& length)
    {
        return length.unit != UiLayoutUnit::Auto && length.unit != UiLayoutUnit::Content;
    }

    float resolveLength(const UiLayoutLength& length,
                        float parentAxis,
                        float surfaceWidth,
                        float surfaceHeight,
                        float contentAxis,
                        float emSize)
    {
        switch (length.unit)
        {
            case UiLayoutUnit::Auto:
                return 0.0f;

            case UiLayoutUnit::Normalized:
                return length.value;

            case UiLayoutUnit::Percent:
            case UiLayoutUnit::ParentWidth:
            case UiLayoutUnit::ParentHeight:
                return parentAxis * length.value;

            case UiLayoutUnit::SurfaceWidth:
                return surfaceWidth * length.value;

            case UiLayoutUnit::SurfaceHeight:
                return surfaceHeight * length.value;

            case UiLayoutUnit::Content:
                return contentAxis;

            case UiLayoutUnit::Em:
                return emSize * length.value;

            case UiLayoutUnit::Pixels:
                return surfaceWidth > 1.0f ? length.value / surfaceWidth : length.value;
        }

        return 0.0f;
    }

    float clampAxis(float value,
                    const UiLayoutLength& minLength,
                    const UiLayoutLength& maxLength,
                    float parentAxis,
                    const UiVec2& surfaceSize,
                    float contentAxis,
                    float emSize)
    {
        float result = std::max(0.0f, value);
        if (minLength.unit != UiLayoutUnit::Auto)
        {
            result = std::max(result,
                              resolveLength(minLength,
                                            parentAxis,
                                            surfaceSize.x,
                                            surfaceSize.y,
                                            contentAxis,
                                            emSize));
        }
        if (maxLength.unit != UiLayoutUnit::Auto)
        {
            result = std::min(result,
                              resolveLength(maxLength,
                                            parentAxis,
                                            surfaceSize.x,
                                            surfaceSize.y,
                                            contentAxis,
                                            emSize));
        }
        return std::max(0.0f, result);
    }

    float alignmentOffset(UiLayoutAlignment alignment, float available, float size)
    {
        switch (alignment)
        {
            case UiLayoutAlignment::Start:
            case UiLayoutAlignment::Stretch:
                return 0.0f;

            case UiLayoutAlignment::Center:
                return (available - size) * 0.5f;

            case UiLayoutAlignment::End:
                return available - size;
        }

        return 0.0f;
    }

    UiVec2 approximateTextSize(const UiTextData& text)
    {
        const float scale = std::max(0.0f, text.scale);
        const float width = static_cast<float>(text.text.size()) * scale * 0.55f;
        const float lineHeight = scale * 1.25f * std::max(0.1f, text.lineHeight);
        const float height = lineHeight * static_cast<float>(std::max(1, text.maxLines == 0 ? 1 : text.maxLines));
        return makeSize(width, height);
    }

    void appendShadowPrimitives(UiVisualList& visualList,
                                UiHandle handle,
                                const UiRect& bounds,
                                const UiRect& clipRect,
                                UiColor color,
                                UiVec2 offset,
                                float blur,
                                float cornerRadius)
    {
        if (color.a <= 0.0f)
            return;

        if (blur <= 0.000001f)
        {
            visualList.primitives.push_back(UiRectPrimitive{
                handle,
                {bounds.x + offset.x, bounds.y + offset.y, bounds.width, bounds.height},
                clipRect,
                color,
                cornerRadius
            });
            return;
        }

        constexpr int ShadowLayers = 6;
        for (int layer = ShadowLayers; layer >= 1; --layer)
        {
            const float t = static_cast<float>(layer) / static_cast<float>(ShadowLayers);
            const float spread = blur * t;
            const float weight = static_cast<float>(ShadowLayers - layer + 1) /
                static_cast<float>(ShadowLayers);
            UiColor layerColor = color;
            layerColor.a *= weight * 0.28f;
            visualList.primitives.push_back(UiRectPrimitive{
                handle,
                {
                    bounds.x + offset.x - spread,
                    bounds.y + offset.y - spread,
                    bounds.width + spread * 2.0f,
                    bounds.height + spread * 2.0f
                },
                clipRect,
                layerColor,
                cornerRadius + spread
            });
        }
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

UiLayerId UiDocument::getNodeLayer(UiHandle handle) const
{
    const UiNode* node = findNode(handle);
    while (node != nullptr)
    {
        for (const auto& [layerId, layer] : layers)
        {
            if (layer.root == node->handle)
                return layerId;
        }

        if (node->parent == UI_INVALID_HANDLE)
            break;
        node = findNode(node->parent);
    }

    return UI_INVALID_LAYER;
}

bool UiDocument::getLayerOrder(UiLayerId layerId, int& outOrder) const
{
    const auto it = layers.find(layerId);
    if (it == layers.end())
        return false;

    outOrder = it->second.order;
    return true;
}

bool UiDocument::canRemoveNode(UiHandle handle) const
{
    return handle != UI_INVALID_HANDLE
        && handle != rootHandle
        && !isLayerRoot(handle)
        && findNode(handle) != nullptr;
}

bool UiDocument::isDescendantOf(UiHandle handle, UiHandle ancestor) const
{
    if (handle == UI_INVALID_HANDLE || ancestor == UI_INVALID_HANDLE)
        return false;

    const UiNode* node = findNode(handle);
    while (node != nullptr)
    {
        if (node->handle == ancestor)
            return true;

        if (node->parent == UI_INVALID_HANDLE)
            return false;

        node = findNode(node->parent);
    }

    return false;
}

std::vector<UiHandle> UiDocument::pathFromRoot(UiHandle handle) const
{
    std::vector<UiHandle> path;
    const UiNode* node = findNode(handle);
    while (node != nullptr)
    {
        path.push_back(node->handle);
        if (node->parent == UI_INVALID_HANDLE)
            break;

        node = findNode(node->parent);
    }

    std::reverse(path.begin(), path.end());
    return path;
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
    markDirty(handle, UiDirtyFlags::Layout | UiDirtyFlags::Visual);
    return true;
}

bool UiDocument::setState(UiHandle handle, const UiStateFlags& state)
{
    UiNode* node = findNode(handle);
    if (!node) return false;
    if (node->state == state) return true;

    node->state = state;
    markDirty(handle, UiDirtyFlags::Layout | UiDirtyFlags::Visual);
    return true;
}

bool UiDocument::setPayload(UiHandle handle, const UiNodePayload& payload)
{
    UiNode* node = findNode(handle);
    if (!node) return false;
    if (node->payload == payload) return true;

    node->payload = payload;
    const bool layoutAffectsPayload = std::holds_alternative<UiTextData>(payload)
        || std::holds_alternative<UiImageData>(payload)
        || std::holds_alternative<UiGridData>(payload);
    markDirty(handle, (layoutAffectsPayload ? UiDirtyFlags::Layout : UiDirtyFlags{}) | UiDirtyFlags::Visual);
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
    updateLayout(inViewportWidth, inViewportHeight, UiTextMeasureCallback{}, nullptr);
}

void UiDocument::updateLayout(float inViewportWidth,
                              float inViewportHeight,
                              const UiTextMeasureCallback& textMeasure,
                              const UiTheme* theme)
{
    viewportWidth  = inViewportWidth;
    viewportHeight = inViewportHeight;
    const UiVec2 surfaceSize = {viewportWidth, viewportHeight};

    UiNode& root = nodes[rootHandle];
    root.computedLayout.bounds      = {0.0f, 0.0f, viewportWidth, viewportHeight};
    root.computedLayout.contentRect = root.computedLayout.bounds;
    root.computedLayout.clipRect    = root.computedLayout.bounds;
    root.computedLayout.measuredSize = surfaceSize;

    for (UiHandle child : root.children)
        measureLayoutRecursive(child, surfaceSize, surfaceSize, textMeasure, theme);

    for (UiHandle child : root.children)
    {
        UiRect childRect = resolveCanvasChildRect(nodes[child], root.computedLayout.contentRect, surfaceSize);
        arrangeLayoutRecursive(child, childRect, root.computedLayout.clipRect, surfaceSize, textMeasure, theme);
    }

    dirtyFlags = static_cast<UiDirtyFlags>(static_cast<uint8_t>(dirtyFlags)
        & ~static_cast<uint8_t>(UiDirtyFlags::Layout));
}

void UiDocument::rebuildVisualList(const UiTheme* theme)
{
    visualList.primitives.clear();
    const UiRect rootClip = nodes[rootHandle].computedLayout.clipRect;
    rebuildVisualRecursive(rootHandle, true, rootClip, theme);

    dirtyFlags = static_cast<UiDirtyFlags>(static_cast<uint8_t>(dirtyFlags)
        & ~static_cast<uint8_t>(UiDirtyFlags::Visual));
}

UiHandle UiDocument::hitTest(float x, float y) const
{
    return hitTestRecursive(rootHandle, x, y, true);
}

UiHandle UiDocument::hitTestLayer(UiLayerId layerId, float x, float y) const
{
    const UiHandle layerRoot = getLayerRoot(layerId);
    if (layerRoot == UI_INVALID_HANDLE)
        return UI_INVALID_HANDLE;

    return hitTestRecursive(layerRoot, x, y, true);
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

UiVec2 UiDocument::measureLayoutRecursive(UiHandle handle,
                                           const UiVec2& availableSize,
                                           const UiVec2& surfaceSize,
                                           const UiTextMeasureCallback& textMeasure,
                                           const UiTheme* theme)
{
    UiNode& node = nodes[handle];
    UiVec2 contentSize = {};

    if (node.type == UiNodeType::Text)
    {
        const UiTextData textData = resolveTextData(node, theme);
        UiTextMeasurement measurement;
        if (textMeasure && textMeasure(handle, textData, measurement))
            contentSize = makeSize(measurement.width, measurement.height);
        else
            contentSize = approximateTextSize(textData);
    }
    else if (node.type == UiNodeType::Image)
    {
        if (const auto* image = std::get_if<UiImageData>(&node.payload))
            contentSize = makeSize(image->imageAspect, 1.0f);
    }
    else if (node.type == UiNodeType::Grid)
    {
        if (const auto* grid = std::get_if<UiGridData>(&node.payload))
            contentSize = makeSize(static_cast<float>(std::max(0, grid->columns)),
                                   static_cast<float>(std::max(0, grid->rows)));
    }

    std::vector<UiVec2> childSizes;
    childSizes.reserve(node.children.size());
    for (UiHandle child : node.children)
    {
        UiNode& childNode = nodes[child];
        if (!childNode.state.visible)
        {
            childNode.computedLayout.measuredSize = {};
            childSizes.push_back({});
            continue;
        }
        childSizes.push_back(measureLayoutRecursive(child, availableSize, surfaceSize, textMeasure, theme));
    }

    UiVec2 childContentSize = {};
    if (!childSizes.empty())
    {
        switch (node.layout.layoutMode)
        {
            case UiLayoutMode::Stack:
            {
                const bool horizontal = node.layout.stackAxis == UiLayoutAxis::Horizontal;
                float main = 0.0f;
                float cross = 0.0f;
                int visibleCount = 0;
                for (size_t i = 0; i < childSizes.size(); ++i)
                {
                    const UiNode& child = nodes[node.children[i]];
                    if (!child.state.visible)
                        continue;
                    ++visibleCount;
                    main += horizontal ? childSizes[i].x : childSizes[i].y;
                    cross = std::max(cross, horizontal ? childSizes[i].y : childSizes[i].x);
                }
                if (visibleCount > 1)
                    main += node.layout.gap * static_cast<float>(visibleCount - 1);
                childContentSize = horizontal ? makeSize(main, cross) : makeSize(cross, main);
                break;
            }

            case UiLayoutMode::Grid:
            {
                const int columns = std::max(1, node.layout.gridColumns);
                const int rows = std::max(1, node.layout.gridRows);
                float cellWidth = 0.0f;
                float cellHeight = 0.0f;
                for (UiVec2 size : childSizes)
                {
                    cellWidth = std::max(cellWidth, size.x);
                    cellHeight = std::max(cellHeight, size.y);
                }
                childContentSize = makeSize(
                    cellWidth * static_cast<float>(columns) +
                        node.layout.gridColumnGap * static_cast<float>(std::max(0, columns - 1)),
                    cellHeight * static_cast<float>(rows) +
                        node.layout.gridRowGap * static_cast<float>(std::max(0, rows - 1)));
                break;
            }

            case UiLayoutMode::Dock:
            {
                for (UiVec2 size : childSizes)
                {
                    childContentSize.x = std::max(childContentSize.x, size.x);
                    childContentSize.y = std::max(childContentSize.y, size.y);
                }
                break;
            }

            case UiLayoutMode::Overlay:
            case UiLayoutMode::Aspect:
            case UiLayoutMode::Constraint:
            case UiLayoutMode::Scroll:
            case UiLayoutMode::Canvas:
            {
                for (UiVec2 size : childSizes)
                {
                    childContentSize.x = std::max(childContentSize.x, size.x);
                    childContentSize.y = std::max(childContentSize.y, size.y);
                }
                break;
            }
        }
    }

    contentSize.x = std::max(contentSize.x, childContentSize.x + node.layout.padding.left + node.layout.padding.right);
    contentSize.y = std::max(contentSize.y, childContentSize.y + node.layout.padding.top + node.layout.padding.bottom);

    const float emSize = node.type == UiNodeType::Text && std::holds_alternative<UiTextData>(node.payload)
        ? std::get<UiTextData>(node.payload).scale
        : 0.02f;
    const UiLayoutConstraints& constraints = node.layout.constraints;

    float width = contentSize.x;
    float height = contentSize.y;
    if (lengthIsExplicit(constraints.preferredWidth))
    {
        width = resolveLength(constraints.preferredWidth,
                              availableSize.x,
                              surfaceSize.x,
                              surfaceSize.y,
                              contentSize.x,
                              emSize);
    }
    else if (constraints.preferredWidth.unit == UiLayoutUnit::Content)
    {
        width = contentSize.x;
    }
    else if (node.layout.widthMode == UiSizeMode::Fixed && node.layout.fixedWidth > 0.0f)
    {
        width = node.layout.fixedWidth;
    }

    if (lengthIsExplicit(constraints.preferredHeight))
    {
        height = resolveLength(constraints.preferredHeight,
                               availableSize.y,
                               surfaceSize.x,
                               surfaceSize.y,
                               contentSize.y,
                               emSize);
    }
    else if (constraints.preferredHeight.unit == UiLayoutUnit::Content)
    {
        height = contentSize.y;
    }
    else if (node.layout.heightMode == UiSizeMode::Fixed && node.layout.fixedHeight > 0.0f)
    {
        height = node.layout.fixedHeight;
    }

    width = clampAxis(width,
                      constraints.minWidth,
                      constraints.maxWidth,
                      availableSize.x,
                      surfaceSize,
                      contentSize.x,
                      emSize);
    height = clampAxis(height,
                       constraints.minHeight,
                       constraints.maxHeight,
                       availableSize.y,
                       surfaceSize,
                       contentSize.y,
                       emSize);

    node.computedLayout.measuredSize = makeSize(width, height);
    return node.computedLayout.measuredSize;
}

void UiDocument::arrangeLayoutRecursive(UiHandle handle,
                                         const UiRect& assignedRect,
                                         const UiRect& inheritedClip,
                                         const UiVec2& surfaceSize,
                                         const UiTextMeasureCallback& textMeasure,
                                         const UiTheme* theme)
{
    UiNode& node = nodes[handle];
    const bool forceClip = node.layout.layoutMode == UiLayoutMode::Scroll;
    node.computedLayout.bounds = resolveBoxInRect(node, assignedRect, surfaceSize);
    node.computedLayout.contentRect = insetRect(node.computedLayout.bounds, node.layout.padding);
    node.computedLayout.clipRect = (node.layout.clipMode == UiClipMode::ClipChildren || forceClip)
        ? intersectRect(inheritedClip, node.computedLayout.bounds)
        : inheritedClip;

    arrangeChildren(handle, surfaceSize, textMeasure, theme);
}

void UiDocument::arrangeChildren(UiHandle handle,
                                 const UiVec2& surfaceSize,
                                 const UiTextMeasureCallback& textMeasure,
                                 const UiTheme* theme)
{
    UiNode& node = nodes[handle];
    switch (node.layout.layoutMode)
    {
        case UiLayoutMode::Canvas:
            arrangeCanvasChildren(handle, surfaceSize, textMeasure, theme, false);
            break;

        case UiLayoutMode::Scroll:
            arrangeCanvasChildren(handle, surfaceSize, textMeasure, theme, true);
            break;

        case UiLayoutMode::Overlay:
        case UiLayoutMode::Aspect:
            arrangeOverlayChildren(handle, surfaceSize, textMeasure, theme);
            break;

        case UiLayoutMode::Stack:
            arrangeStackChildren(handle, surfaceSize, textMeasure, theme);
            break;

        case UiLayoutMode::Grid:
            arrangeGridChildren(handle, surfaceSize, textMeasure, theme);
            break;

        case UiLayoutMode::Dock:
            arrangeDockChildren(handle, surfaceSize, textMeasure, theme);
            break;

        case UiLayoutMode::Constraint:
            arrangeConstraintChildren(handle, surfaceSize, textMeasure, theme);
            break;
    }
}

void UiDocument::arrangeCanvasChildren(UiHandle handle,
                                       const UiVec2& surfaceSize,
                                       const UiTextMeasureCallback& textMeasure,
                                       const UiTheme* theme,
                                       bool forceClip)
{
    UiNode& node = nodes[handle];
    UiRect parentRect = node.computedLayout.contentRect;
    parentRect.x += node.layout.contentOffset.x;
    parentRect.y += node.layout.contentOffset.y;

    const UiRect childClip = forceClip ? intersectRect(node.computedLayout.clipRect, node.computedLayout.bounds)
                                       : node.computedLayout.clipRect;
    for (UiHandle child : node.children)
    {
        UiRect childRect = resolveCanvasChildRect(nodes[child], parentRect, surfaceSize);
        arrangeLayoutRecursive(child, childRect, childClip, surfaceSize, textMeasure, theme);
    }
}

void UiDocument::arrangeOverlayChildren(UiHandle handle,
                                        const UiVec2& surfaceSize,
                                        const UiTextMeasureCallback& textMeasure,
                                        const UiTheme* theme)
{
    UiNode& node = nodes[handle];
    for (UiHandle child : node.children)
        arrangeLayoutRecursive(child, node.computedLayout.contentRect, node.computedLayout.clipRect, surfaceSize, textMeasure, theme);
}

void UiDocument::arrangeStackChildren(UiHandle handle,
                                      const UiVec2& surfaceSize,
                                      const UiTextMeasureCallback& textMeasure,
                                      const UiTheme* theme)
{
    UiNode& node = nodes[handle];
    const bool horizontal = node.layout.stackAxis == UiLayoutAxis::Horizontal;
    const UiRect content = node.computedLayout.contentRect;
    const float availableMain = horizontal ? content.width : content.height;
    const float availableCross = horizontal ? content.height : content.width;

    float fixedMain = 0.0f;
    float growTotal = 0.0f;
    int visibleCount = 0;
    for (UiHandle child : node.children)
    {
        const UiNode& childNode = nodes[child];
        if (!childNode.state.visible)
            continue;

        ++visibleCount;
        fixedMain += horizontal
            ? childNode.computedLayout.measuredSize.x + childNode.layout.margin.left + childNode.layout.margin.right
            : childNode.computedLayout.measuredSize.y + childNode.layout.margin.top + childNode.layout.margin.bottom;
        growTotal += std::max(0.0f, childNode.layout.constraints.grow);
    }

    const float gapTotal = visibleCount > 1
        ? node.layout.gap * static_cast<float>(visibleCount - 1)
        : 0.0f;
    const float extraMain = std::max(0.0f, availableMain - fixedMain - gapTotal);
    float stackMain = fixedMain + gapTotal + (growTotal > 0.0f ? extraMain : 0.0f);
    float cursor = horizontal ? content.x : content.y;
    cursor += alignmentOffset(node.layout.mainAxisAlignment, availableMain, stackMain);

    for (UiHandle child : node.children)
    {
        UiNode& childNode = nodes[child];
        if (!childNode.state.visible)
            continue;

        UiVec2 size = childNode.computedLayout.measuredSize;
        const float grow = std::max(0.0f, childNode.layout.constraints.grow);
        if (growTotal > 0.0f && grow > 0.0f)
        {
            if (horizontal)
                size.x += extraMain * (grow / growTotal);
            else
                size.y += extraMain * (grow / growTotal);
        }

        if (node.layout.crossAxisAlignment == UiLayoutAlignment::Stretch)
        {
            if (horizontal)
                size.y = std::max(0.0f, availableCross - childNode.layout.margin.top - childNode.layout.margin.bottom);
            else
                size.x = std::max(0.0f, availableCross - childNode.layout.margin.left - childNode.layout.margin.right);
        }

        UiRect slot;
        if (horizontal)
        {
            slot = {
                cursor,
                content.y,
                size.x + childNode.layout.margin.left + childNode.layout.margin.right,
                availableCross
            };
            cursor += slot.width + node.layout.gap;
        }
        else
        {
            slot = {
                content.x,
                cursor,
                availableCross,
                size.y + childNode.layout.margin.top + childNode.layout.margin.bottom
            };
            cursor += slot.height + node.layout.gap;
        }

        arrangeLayoutRecursive(child, slot, node.computedLayout.clipRect, surfaceSize, textMeasure, theme);
    }
}

void UiDocument::arrangeGridChildren(UiHandle handle,
                                     const UiVec2& surfaceSize,
                                     const UiTextMeasureCallback& textMeasure,
                                     const UiTheme* theme)
{
    UiNode& node = nodes[handle];
    const UiRect content = node.computedLayout.contentRect;
    const int columns = std::max(1, node.layout.gridColumns);
    const int rows = std::max(1, node.layout.gridRows);
    const float columnGapTotal = node.layout.gridColumnGap * static_cast<float>(std::max(0, columns - 1));
    const float rowGapTotal = node.layout.gridRowGap * static_cast<float>(std::max(0, rows - 1));
    const float cellWidth = std::max(0.0f, (content.width - columnGapTotal) / static_cast<float>(columns));
    const float cellHeight = std::max(0.0f, (content.height - rowGapTotal) / static_cast<float>(rows));

    for (UiHandle child : node.children)
    {
        UiNode& childNode = nodes[child];
        const int column = std::clamp(childNode.layout.gridColumn, 0, columns - 1);
        const int row = std::clamp(childNode.layout.gridRow, 0, rows - 1);
        const int columnSpan = std::clamp(childNode.layout.gridColumnSpan, 1, columns - column);
        const int rowSpan = std::clamp(childNode.layout.gridRowSpan, 1, rows - row);

        UiRect slot;
        slot.x = content.x + static_cast<float>(column) * (cellWidth + node.layout.gridColumnGap);
        slot.y = content.y + static_cast<float>(row) * (cellHeight + node.layout.gridRowGap);
        slot.width = cellWidth * static_cast<float>(columnSpan) +
            node.layout.gridColumnGap * static_cast<float>(columnSpan - 1);
        slot.height = cellHeight * static_cast<float>(rowSpan) +
            node.layout.gridRowGap * static_cast<float>(rowSpan - 1);
        arrangeLayoutRecursive(child, slot, node.computedLayout.clipRect, surfaceSize, textMeasure, theme);
    }
}

void UiDocument::arrangeDockChildren(UiHandle handle,
                                     const UiVec2& surfaceSize,
                                     const UiTextMeasureCallback& textMeasure,
                                     const UiTheme* theme)
{
    UiNode& node = nodes[handle];
    UiRect remaining = node.computedLayout.contentRect;

    for (UiHandle child : node.children)
    {
        UiNode& childNode = nodes[child];
        if (!childNode.state.visible)
            continue;

        UiRect slot = remaining;
        switch (childNode.layout.dock)
        {
            case UiDockEdge::Left:
                slot.width = std::min(remaining.width, childNode.computedLayout.measuredSize.x);
                remaining.x += slot.width + node.layout.gap;
                remaining.width = std::max(0.0f, remaining.width - slot.width - node.layout.gap);
                break;

            case UiDockEdge::Right:
                slot.width = std::min(remaining.width, childNode.computedLayout.measuredSize.x);
                slot.x = remaining.x + remaining.width - slot.width;
                remaining.width = std::max(0.0f, remaining.width - slot.width - node.layout.gap);
                break;

            case UiDockEdge::Top:
                slot.height = std::min(remaining.height, childNode.computedLayout.measuredSize.y);
                remaining.y += slot.height + node.layout.gap;
                remaining.height = std::max(0.0f, remaining.height - slot.height - node.layout.gap);
                break;

            case UiDockEdge::Bottom:
                slot.height = std::min(remaining.height, childNode.computedLayout.measuredSize.y);
                slot.y = remaining.y + remaining.height - slot.height;
                remaining.height = std::max(0.0f, remaining.height - slot.height - node.layout.gap);
                break;

            case UiDockEdge::Fill:
                slot = remaining;
                break;
        }

        arrangeLayoutRecursive(child, slot, node.computedLayout.clipRect, surfaceSize, textMeasure, theme);
    }
}

void UiDocument::arrangeConstraintChildren(UiHandle handle,
                                           const UiVec2& surfaceSize,
                                           const UiTextMeasureCallback& textMeasure,
                                           const UiTheme* theme)
{
    UiNode& node = nodes[handle];
    for (UiHandle child : node.children)
        arrangeLayoutRecursive(child, node.computedLayout.contentRect, node.computedLayout.clipRect, surfaceSize, textMeasure, theme);
}

UiRect UiDocument::resolveCanvasChildRect(const UiNode& child,
                                          const UiRect& parentRect,
                                          const UiVec2& surfaceSize) const
{
    float x = parentRect.x;
    float y = parentRect.y;
    float width = child.layout.fixedWidth;
    float height = child.layout.fixedHeight;

    if (child.layout.positionMode == UiPositionMode::Anchored)
    {
        const float left = parentRect.x + parentRect.width * child.layout.anchorMin.x + child.layout.offsetMin.x;
        const float top = parentRect.y + parentRect.height * child.layout.anchorMin.y + child.layout.offsetMin.y;
        x = left;
        y = top;

        if (child.layout.widthMode == UiSizeMode::FromAnchors)
        {
            const float right = parentRect.x + parentRect.width * child.layout.anchorMax.x + child.layout.offsetMax.x;
            width = std::max(0.0f, right - left);
        }

        if (child.layout.heightMode == UiSizeMode::FromAnchors)
        {
            const float bottom = parentRect.y + parentRect.height * child.layout.anchorMax.y + child.layout.offsetMax.y;
            height = std::max(0.0f, bottom - top);
        }
    }
    else
    {
        x = parentRect.x + child.layout.offsetMin.x;
        y = parentRect.y + child.layout.offsetMin.y;
    }

    if (width <= 0.0f)
        width = child.computedLayout.measuredSize.x;
    if (height <= 0.0f)
        height = child.computedLayout.measuredSize.y;

    return {x, y, width, height};
}

UiRect UiDocument::resolveBoxInRect(const UiNode& node,
                                    const UiRect& slot,
                                    const UiVec2& surfaceSize) const
{
    const UiRect innerSlot = applyMargin(slot, node.layout.margin);
    const UiLayoutConstraints& constraints = node.layout.constraints;
    const float emSize = node.type == UiNodeType::Text && std::holds_alternative<UiTextData>(node.payload)
        ? std::get<UiTextData>(node.payload).scale
        : 0.02f;

    float width = innerSlot.width;
    float height = innerSlot.height;

    if (lengthIsExplicit(constraints.preferredWidth))
    {
        width = resolveLength(constraints.preferredWidth,
                              innerSlot.width,
                              surfaceSize.x,
                              surfaceSize.y,
                              node.computedLayout.measuredSize.x,
                              emSize);
    }
    else if (constraints.preferredWidth.unit == UiLayoutUnit::Content)
    {
        width = node.computedLayout.measuredSize.x;
    }
    else if (constraints.horizontalAlignment != UiLayoutAlignment::Stretch)
    {
        width = std::min(width, node.computedLayout.measuredSize.x);
    }

    if (lengthIsExplicit(constraints.preferredHeight))
    {
        height = resolveLength(constraints.preferredHeight,
                               innerSlot.height,
                               surfaceSize.x,
                               surfaceSize.y,
                               node.computedLayout.measuredSize.y,
                               emSize);
    }
    else if (constraints.preferredHeight.unit == UiLayoutUnit::Content)
    {
        height = node.computedLayout.measuredSize.y;
    }
    else if (constraints.verticalAlignment != UiLayoutAlignment::Stretch)
    {
        height = std::min(height, node.computedLayout.measuredSize.y);
    }

    width = clampAxis(width,
                      constraints.minWidth,
                      constraints.maxWidth,
                      innerSlot.width,
                      surfaceSize,
                      node.computedLayout.measuredSize.x,
                      emSize);
    height = clampAxis(height,
                       constraints.minHeight,
                       constraints.maxHeight,
                       innerSlot.height,
                       surfaceSize,
                       node.computedLayout.measuredSize.y,
                       emSize);

    const float aspect = constraints.aspectRatio;
    if (aspect > 0.0f && width > 0.0f && height > 0.0f)
    {
        if (width / height > aspect)
            width = height * aspect;
        else
            height = width / aspect;
    }

    width = std::min(width, innerSlot.width);
    height = std::min(height, innerSlot.height);

    const float x = innerSlot.x +
        alignmentOffset(constraints.horizontalAlignment, innerSlot.width, width);
    const float y = innerSlot.y +
        alignmentOffset(constraints.verticalAlignment, innerSlot.height, height);
    return {x, y, std::max(0.0f, width), std::max(0.0f, height)};
}

UiComputedStyle UiDocument::resolveComputedStyle(const UiNode& node, const UiTheme* theme) const
{
    UiComputedStyle computed;
    if (theme != nullptr)
        theme->resolveNodeStyle(node.style, node.state, computed);
    return computed;
}

UiTextData UiDocument::resolveTextData(const UiNode& node, const UiTheme* theme) const
{
    UiTextData data = std::get<UiTextData>(node.payload);
    const UiComputedStyle style = resolveComputedStyle(node, theme);

    if (style.hasForegroundColor)
        data.color = style.foregroundColor;
    if (style.hasTypography)
    {
        if (!style.typography.fontAsset.empty())
            data.fontAsset = style.typography.fontAsset;
        data.scale = style.typography.scale;
        data.lineHeight = style.typography.lineHeight;
        data.letterSpacing = style.typography.letterSpacing;
    }
    if (style.hasOpacity)
        data.color.a *= style.opacity;

    return data;
}

UiColor UiDocument::resolveFillColor(const UiNode& node, const UiTheme* theme, UiColor fallback) const
{
    const UiComputedStyle style = resolveComputedStyle(node, theme);
    UiColor color = fallback;

    if (style.hasSkin && style.skin.type == UiPanelSkinType::SolidColor)
        color = style.skin.color;
    else if (style.hasBackgroundColor)
        color = style.backgroundColor;

    if (style.hasOpacity)
        color.a *= style.opacity;
    return color;
}

UiPanelSkin UiDocument::resolvePanelSkinForNode(const UiNode& node,
                                                const UiTheme* theme,
                                                UiPanelSkin fallback) const
{
    const UiComputedStyle style = resolveComputedStyle(node, theme);
    UiPanelSkin skin = style.hasSkin ? style.skin : fallback;
    if (style.hasPadding)
        skin.contentPadding = style.padding;
    if (style.hasOpacity)
    {
        skin.color.a *= style.opacity;
        skin.tint.a *= style.opacity;
    }
    return skin;
}

void UiDocument::rebuildVisualRecursive(UiHandle handle,
                                        bool parentVisible,
                                        const UiRect& inheritedClip,
                                        const UiTheme* theme)
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
            UiPanelSkin fallback;
            fallback.type = UiPanelSkinType::SolidColor;
            fallback.color = data.color;
            const UiPanelSkin skin = resolvePanelSkinForNode(node, theme, fallback);
            if (skin.type == UiPanelSkinType::Image && !skin.imageAsset.empty())
            {
                visualList.primitives.push_back(UiImagePrimitive{
                    node.handle,
                    node.computedLayout.bounds,
                    effectiveClip,
                    skin.imageAsset,
                    0,
                    skin.tint,
                    1.0f,
                    0.0f
                });
            }
            else if (skin.type == UiPanelSkinType::NineSlice && !skin.imageAsset.empty())
            {
                visualList.primitives.push_back(UiNineSlicePrimitive{
                    node.handle,
                    node.computedLayout.bounds,
                    effectiveClip,
                    skin.imageAsset,
                    skin.tint,
                    skin.slice
                });
            }
            else
            {
                const UiRect bounds = node.computedLayout.bounds;
                const UiComputedStyle style = resolveComputedStyle(node, theme);
                const float borderThickness = style.hasBorderWidth
                    ? style.borderWidth
                    : std::max(0.0f, data.borderThickness);
                const float cornerRadius = style.hasBorderRadius
                    ? style.borderRadius
                    : std::max(0.0f, data.cornerRadius);
                UiColor borderColor = style.hasBorderColor ? style.borderColor : data.borderColor;
                UiColor shadowColor = style.hasShadowColor ? style.shadowColor : data.shadowColor;
                const UiVec2 shadowOffset = style.hasShadowOffset ? style.shadowOffset : data.shadowOffset;
                const float shadowBlur = style.hasShadowBlur ? style.shadowBlur : data.shadowBlur;
                if (style.hasOpacity)
                {
                    borderColor.a *= style.opacity;
                    shadowColor.a *= style.opacity;
                }
                const UiColor fillColor = resolveFillColor(node, theme, data.color);
                if (shadowColor.a > 0.0f &&
                    (std::fabs(shadowOffset.x) > 0.000001f ||
                     std::fabs(shadowOffset.y) > 0.000001f ||
                     shadowBlur > 0.000001f))
                {
                    appendShadowPrimitives(visualList,
                                           node.handle,
                                           bounds,
                                           effectiveClip,
                                           shadowColor,
                                           shadowOffset,
                                           shadowBlur,
                                           cornerRadius);
                }

                if (borderThickness > 0.0f && borderColor.a > 0.0f)
                {
                    if (cornerRadius > 0.0f && fillColor.a > 0.0f)
                    {
                        visualList.primitives.push_back(UiRectPrimitive{
                            node.handle,
                            bounds,
                            effectiveClip,
                            borderColor,
                            cornerRadius
                        });

                        const float insetX = std::min(borderThickness, bounds.width * 0.5f);
                        const float insetY = std::min(borderThickness, bounds.height * 0.5f);
                        const UiRect innerBounds = {
                            bounds.x + insetX,
                            bounds.y + insetY,
                            std::max(0.0f, bounds.width - insetX * 2.0f),
                            std::max(0.0f, bounds.height - insetY * 2.0f)
                        };
                        if (innerBounds.width > 0.0f && innerBounds.height > 0.0f)
                        {
                            visualList.primitives.push_back(UiRectPrimitive{
                                node.handle,
                                innerBounds,
                                effectiveClip,
                                fillColor,
                                std::max(0.0f, cornerRadius - std::min(insetX, insetY))
                            });
                        }
                    }
                    else
                    {
                        visualList.primitives.push_back(UiRectPrimitive{
                            node.handle,
                            bounds,
                            effectiveClip,
                            fillColor,
                            cornerRadius
                        });

                        const float thicknessX = std::min(borderThickness, bounds.width);
                        const float thicknessY = std::min(borderThickness, bounds.height);
                        visualList.primitives.push_back(UiRectPrimitive{
                            node.handle,
                            {bounds.x, bounds.y, bounds.width, thicknessY},
                            effectiveClip,
                            borderColor
                        });
                        visualList.primitives.push_back(UiRectPrimitive{
                            node.handle,
                            {bounds.x, bounds.y + std::max(0.0f, bounds.height - thicknessY), bounds.width, thicknessY},
                            effectiveClip,
                            borderColor
                        });
                        visualList.primitives.push_back(UiRectPrimitive{
                            node.handle,
                            {bounds.x, bounds.y, thicknessX, bounds.height},
                            effectiveClip,
                            borderColor
                        });
                        visualList.primitives.push_back(UiRectPrimitive{
                            node.handle,
                            {bounds.x + std::max(0.0f, bounds.width - thicknessX), bounds.y, thicknessX, bounds.height},
                            effectiveClip,
                            borderColor
                        });
                    }
                }
                else
                {
                    visualList.primitives.push_back(UiRectPrimitive{
                        node.handle,
                        bounds,
                        effectiveClip,
                        fillColor,
                        cornerRadius
                    });
                }
            }
            break;
        }

        case UiNodeType::Image:
        {
            const auto& data = std::get<UiImageData>(node.payload);
            UiPanelSkin fallback;
            fallback.type = UiPanelSkinType::Image;
            fallback.imageAsset = data.imageAsset;
            fallback.tint = data.tint;
            const UiPanelSkin skin = resolvePanelSkinForNode(node, theme, fallback);
            visualList.primitives.push_back(UiImagePrimitive{
                node.handle,
                node.computedLayout.bounds,
                effectiveClip,
                skin.type == UiPanelSkinType::Image ? skin.imageAsset : data.imageAsset,
                data.textureID,
                skin.type == UiPanelSkinType::Image ? skin.tint : data.tint,
                data.imageAspect,
                data.rotation
            });
            break;
        }

        case UiNodeType::NineSlice:
        {
            const auto& data = std::get<UiNineSliceData>(node.payload);
            UiPanelSkin fallback;
            fallback.type = UiPanelSkinType::NineSlice;
            fallback.imageAsset = data.imageAsset;
            fallback.tint = data.tint;
            fallback.slice = data.slice;
            const UiPanelSkin skin = resolvePanelSkinForNode(node, theme, fallback);
            visualList.primitives.push_back(UiNineSlicePrimitive{
                node.handle,
                node.computedLayout.bounds,
                effectiveClip,
                skin.type == UiPanelSkinType::NineSlice ? skin.imageAsset : data.imageAsset,
                skin.type == UiPanelSkinType::NineSlice ? skin.tint : data.tint,
                skin.type == UiPanelSkinType::NineSlice ? skin.slice : data.slice
            });
            break;
        }

        case UiNodeType::Text:
        {
            const UiTextData data = resolveTextData(node, theme);
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
                data.maxLines,
                data.lineHeight,
                data.letterSpacing
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
                resolveFillColor(node, theme, data.color),
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
                resolveFillColor(node, theme, data.color),
                data.segments
            });
            break;
        }
    }

    for (UiHandle child : node.children)
        rebuildVisualRecursive(child, visible, effectiveClip, theme);
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
