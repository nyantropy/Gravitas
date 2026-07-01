#pragma once

#include <unordered_map>

#include "UiDocument.h"
#include "UiDragTypes.h"
#include "UiFocusManager.h"
#include "UiInteraction.h"
#include "UiModalManager.h"

struct UiDragSource
{
    UiHandle handle = UI_INVALID_HANDLE;
    UiDragSourceDesc desc;
};

struct UiDropTarget
{
    UiHandle handle = UI_INVALID_HANDLE;
    UiDropTargetDesc desc;
};

struct UiDragContext
{
    UiDragState state = UiDragState::Idle;
    UiPointerId pointerId = UI_PRIMARY_POINTER;
    UiHandle source = UI_INVALID_HANDLE;
    UiHandle target = UI_INVALID_HANDLE;
    UiDragPayload payload;
    float startX = 0.0f;
    float startY = 0.0f;
    float pointerX = 0.0f;
    float pointerY = 0.0f;
};

class UiDragDropManager
{
public:
    void clear();

    bool registerSource(UiHandle handle, const UiDragSourceDesc& desc);
    bool unregisterSource(UiHandle handle);
    bool registerTarget(UiHandle handle, const UiDropTargetDesc& desc);
    bool unregisterTarget(UiHandle handle);
    void unregisterSubtree(const UiDocument& document, UiHandle root);
    void pruneInvalidNodes(UiDocument& document, UiFocusManager& focusManager);

    const UiDragSource* findSource(UiHandle handle) const;
    const UiDropTarget* findTarget(UiHandle handle) const;

    bool active() const { return context.state != UiDragState::Idle; }
    bool dragging() const { return context.state == UiDragState::Dragging; }
    const UiDragContext& dragContext() const { return context; }

    UiDragFrameResult update(UiDocument& document,
                             UiFocusManager& focusManager,
                             const UiModalManager& modalManager,
                             const UiInputFrame& input,
                             UiHandle hovered,
                             UiHandle pointerTarget,
                             UiPointerId pointerId = UI_PRIMARY_POINTER);

private:
    UiHandle resolveSource(const UiDocument& document,
                           const UiModalManager& modalManager,
                           UiHandle handle) const;
    UiHandle resolveTarget(const UiDocument& document,
                           const UiModalManager& modalManager,
                           UiHandle handle,
                           const UiDragPayload& payload) const;
    bool isNodeVisibleAndEnabled(const UiDocument& document, UiHandle handle) const;
    bool isLayerAvailableForPointer(const UiDocument& document,
                                    const UiModalManager& modalManager,
                                    UiHandle handle) const;
    bool targetAcceptsPayload(const UiDropTargetDesc& desc, const UiDragPayload& payload) const;
    bool shouldStartDrag(const UiInputFrame& input) const;
    UiDragFrameResult snapshotResult(bool includeActive) const;
    UiDragFrameResult cancel(UiDocument& document,
                             UiFocusManager& focusManager,
                             bool markCancelled,
                             UiPointerId pointerId);
    void clearContext(UiFocusManager& focusManager, UiPointerId pointerId);

    std::unordered_map<UiHandle, UiDragSource> sources;
    std::unordered_map<UiHandle, UiDropTarget> targets;
    UiDragContext context;
};
