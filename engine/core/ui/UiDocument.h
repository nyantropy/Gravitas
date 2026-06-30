#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "UiLayer.h"
#include "UiNode.h"
#include "UiVisualPrimitives.h"

class UiDocument
{
public:
    UiDocument();
    void clear();

    UiHandle getRoot() const { return defaultLayerRoot; }
    UiHandle getDocumentRoot() const { return rootHandle; }

    UiLayerId createLayer(const std::string& name, int order);
    bool      removeLayer(UiLayerId layerId);
    bool      setLayerOrder(UiLayerId layerId, int order);
    bool      setLayerState(UiLayerId layerId, const UiLayerState& state);
    UiHandle  getLayerRoot(UiLayerId layerId) const;
    UiLayerId getDefaultLayer() const { return UI_DEFAULT_LAYER; }
    bool      canRemoveNode(UiHandle handle) const;

    UiHandle createNode(UiNodeType type, UiHandle parent = UI_INVALID_HANDLE);
    bool     removeNode(UiHandle handle);
    bool     reparentNode(UiHandle handle, UiHandle newParent);

    UiNode*       findNode(UiHandle handle);
    const UiNode* findNode(UiHandle handle) const;

    bool setLayout(UiHandle handle, const UiLayoutSpec& layout);
    bool setStyle(UiHandle handle, const UiStyle& style);
    bool setState(UiHandle handle, const UiStateFlags& state);
    bool setPayload(UiHandle handle, const UiNodePayload& payload);

    void markDirty(UiHandle handle, UiDirtyFlags flags);
    void markSubtreeDirty(UiHandle handle, UiDirtyFlags flags);
    void markAllDirty(UiDirtyFlags flags);

    void setViewportSize(float viewportWidth, float viewportHeight);
    void updateLayout(float viewportWidth, float viewportHeight);
    void rebuildVisualList();

    const UiVisualList& getVisualList() const { return visualList; }
    UiDirtyFlags        getDirtyFlags() const { return dirtyFlags; }
    uint64_t            getVisualRevision() const { return visualRevision; }
    size_t              getNodeCount() const { return nodes.size(); }
    UiHandle            hitTest(float x, float y) const;

private:
    UiHandle allocHandle();
    void     removeNodeRecursive(UiHandle handle);
    void     detachFromParent(UiHandle handle);
    void     appendChild(UiHandle parent, UiHandle child);
    void     sortLayerRoots();
    void     createLayerRoot(UiLayer& layer);
    bool     isLayerRoot(UiHandle handle) const;
    void     computeLayoutRecursive(UiHandle handle, const UiComputedLayout* parentLayout);
    void     rebuildVisualRecursive(UiHandle handle, bool parentVisible, const UiRect& inheritedClip);
    UiHandle hitTestRecursive(UiHandle handle, float x, float y, bool parentVisible) const;

    UiHandle                             nextHandle  = 1;
    UiLayerId                            nextLayerId = UI_DEFAULT_LAYER + 1;
    UiHandle                             rootHandle  = UI_INVALID_HANDLE;
    UiHandle                             defaultLayerRoot = UI_INVALID_HANDLE;
    UiDirtyFlags                         dirtyFlags  = UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual;
    uint64_t                             visualRevision = 1;
    std::unordered_map<UiHandle, UiNode> nodes;
    std::unordered_map<UiLayerId, UiLayer> layers;
    UiVisualList                         visualList;
    float                                viewportWidth  = 0.0f;
    float                                viewportHeight = 0.0f;
};
