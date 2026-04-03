#pragma once

#include <unordered_map>

#include "UiNode.h"
#include "UiVisualPrimitives.h"

class UiDocument
{
public:
    UiDocument();
    void clear();

    UiHandle getRoot() const { return rootHandle; }

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

private:
    UiHandle allocHandle();
    void     removeNodeRecursive(UiHandle handle);
    void     detachFromParent(UiHandle handle);
    void     appendChild(UiHandle parent, UiHandle child);
    void     computeLayoutRecursive(UiHandle handle, const UiComputedLayout* parentLayout);
    void     rebuildVisualRecursive(UiHandle handle, bool parentVisible, const UiRect& inheritedClip);

    UiHandle                             nextHandle  = 1;
    UiHandle                             rootHandle  = UI_INVALID_HANDLE;
    UiDirtyFlags                         dirtyFlags  = UiDirtyFlags::Structure | UiDirtyFlags::Layout | UiDirtyFlags::Visual;
    std::unordered_map<UiHandle, UiNode> nodes;
    UiVisualList                         visualList;
    float                                viewportWidth  = 0.0f;
    float                                viewportHeight = 0.0f;
};
