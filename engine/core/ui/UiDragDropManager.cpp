#include "UiDragDropManager.h"

#include <algorithm>
#include <cmath>

#include "UiNode.h"

namespace
{
    bool sameHandle(UiHandle lhs, UiHandle rhs)
    {
        return lhs != UI_INVALID_HANDLE && lhs == rhs;
    }
}

void UiDragDropManager::clear()
{
    sources.clear();
    targets.clear();
    context = {};
}

bool UiDragDropManager::registerSource(UiHandle handle, const UiDragSourceDesc& desc)
{
    if (handle == UI_INVALID_HANDLE)
        return false;

    sources[handle] = UiDragSource{handle, desc};
    return true;
}

bool UiDragDropManager::unregisterSource(UiHandle handle)
{
    return sources.erase(handle) > 0;
}

bool UiDragDropManager::registerTarget(UiHandle handle, const UiDropTargetDesc& desc)
{
    if (handle == UI_INVALID_HANDLE)
        return false;

    targets[handle] = UiDropTarget{handle, desc};
    return true;
}

bool UiDragDropManager::unregisterTarget(UiHandle handle)
{
    return targets.erase(handle) > 0;
}

void UiDragDropManager::unregisterSubtree(const UiDocument& document, UiHandle root)
{
    if (root == UI_INVALID_HANDLE)
        return;

    for (auto it = sources.begin(); it != sources.end();)
    {
        if (it->first == root || document.isDescendantOf(it->first, root))
            it = sources.erase(it);
        else
            ++it;
    }

    for (auto it = targets.begin(); it != targets.end();)
    {
        if (it->first == root || document.isDescendantOf(it->first, root))
            it = targets.erase(it);
        else
            ++it;
    }
}

void UiDragDropManager::pruneInvalidNodes(UiDocument& document, UiFocusManager& focusManager)
{
    for (auto it = sources.begin(); it != sources.end();)
    {
        if (document.findNode(it->first) == nullptr)
            it = sources.erase(it);
        else
            ++it;
    }

    for (auto it = targets.begin(); it != targets.end();)
    {
        if (document.findNode(it->first) == nullptr)
            it = targets.erase(it);
        else
            ++it;
    }

    if (context.source != UI_INVALID_HANDLE && document.findNode(context.source) == nullptr)
        cancel(document, focusManager, true, context.pointerId);
    else if (context.target != UI_INVALID_HANDLE && document.findNode(context.target) == nullptr)
        context.target = UI_INVALID_HANDLE;
}

const UiDragSource* UiDragDropManager::findSource(UiHandle handle) const
{
    const auto it = sources.find(handle);
    return it == sources.end() ? nullptr : &it->second;
}

const UiDropTarget* UiDragDropManager::findTarget(UiHandle handle) const
{
    const auto it = targets.find(handle);
    return it == targets.end() ? nullptr : &it->second;
}

UiDragFrameResult UiDragDropManager::update(UiDocument& document,
                                            UiFocusManager& focusManager,
                                            const UiModalManager& modalManager,
                                            const UiInputFrame& input,
                                            UiHandle hovered,
                                            UiHandle pointerTarget,
                                            UiPointerId pointerId)
{
    pruneInvalidNodes(document, focusManager);

    if (context.state == UiDragState::Idle)
    {
        if (!input.primaryPressed)
            return {};

        const UiHandle source = resolveSource(document, modalManager, pointerTarget);
        const UiDragSource* sourceState = findSource(source);
        if (sourceState == nullptr || !sourceState->desc.enabled)
            return {};

        context = {};
        context.state = UiDragState::Armed;
        context.pointerId = pointerId;
        context.source = source;
        context.payload = sourceState->desc.payload;
        context.startX = input.pointerX;
        context.startY = input.pointerY;
        context.pointerX = input.pointerX;
        context.pointerY = input.pointerY;
        if (sourceState->desc.capturePointer)
            focusManager.capturePointer(document, pointerId, source);

        if (sourceState->desc.startThreshold <= 0.0f)
        {
            context.state = UiDragState::Dragging;
            context.target = resolveTarget(document, modalManager, hovered, context.payload);
            UiDragFrameResult result = snapshotResult(true);
            result.started = true;
            result.enteredTarget = context.target != UI_INVALID_HANDLE;
            result.targetChanged = result.enteredTarget;
            result.moved = true;
            return result;
        }

        return snapshotResult(true);
    }

    if (context.pointerId != pointerId)
        return snapshotResult(true);

    const float previousPointerX = context.pointerX;
    const float previousPointerY = context.pointerY;
    UiDragFrameResult result = snapshotResult(true);
    result.pointerX = input.pointerX;
    result.pointerY = input.pointerY;
    result.deltaX = input.pointerX - previousPointerX;
    result.deltaY = input.pointerY - previousPointerY;
    context.pointerX = input.pointerX;
    context.pointerY = input.pointerY;

    if (input.cancelPressed)
        return cancel(document, focusManager, true, pointerId);

    if (context.source == UI_INVALID_HANDLE || !isNodeVisibleAndEnabled(document, context.source))
        return cancel(document, focusManager, true, pointerId);

    const UiDragSource* sourceState = findSource(context.source);
    if (sourceState == nullptr || !sourceState->desc.enabled)
        return cancel(document, focusManager, true, pointerId);

    bool startedThisFrame = false;
    if (context.state == UiDragState::Armed)
    {
        if (input.primaryReleased || (!input.primaryDown && !input.primaryPressed))
        {
            clearContext(focusManager, pointerId);
            return {};
        }

        if (!shouldStartDrag(input))
            return result;

        context.state = UiDragState::Dragging;
        startedThisFrame = true;
    }

    if (context.state != UiDragState::Dragging)
        return result;

    const UiHandle previousTarget = context.target;
    const UiHandle nextTarget = resolveTarget(document, modalManager, hovered, context.payload);
    context.target = nextTarget;

    result = snapshotResult(true);
    result.pointerX = input.pointerX;
    result.pointerY = input.pointerY;
    result.deltaX = input.pointerX - previousPointerX;
    result.deltaY = input.pointerY - previousPointerY;
    result.started = startedThisFrame;
    result.moved = startedThisFrame ||
                   std::fabs(result.deltaX) > 0.0f ||
                   std::fabs(result.deltaY) > 0.0f;
    result.previousTarget = previousTarget;
    result.targetChanged = previousTarget != nextTarget;
    result.leftTarget = previousTarget != UI_INVALID_HANDLE && previousTarget != nextTarget;
    result.enteredTarget = nextTarget != UI_INVALID_HANDLE && previousTarget != nextTarget;

    if (input.primaryReleased || (!input.primaryDown && !input.primaryPressed))
    {
        result.ended = true;
        if (context.target != UI_INVALID_HANDLE)
        {
            result.dropped = true;
            result.accepted = true;
        }
        else
        {
            result.rejected = true;
        }
        clearContext(focusManager, pointerId);
    }

    return result;
}

UiHandle UiDragDropManager::resolveSource(const UiDocument& document,
                                          const UiModalManager& modalManager,
                                          UiHandle handle) const
{
    UiHandle current = handle;
    while (current != UI_INVALID_HANDLE)
    {
        const auto it = sources.find(current);
        if (it != sources.end() &&
            it->second.desc.enabled &&
            isNodeVisibleAndEnabled(document, current) &&
            isLayerAvailableForPointer(document, modalManager, current))
        {
            return current;
        }

        const UiNode* node = document.findNode(current);
        if (node == nullptr)
            return UI_INVALID_HANDLE;
        current = node->parent;
    }
    return UI_INVALID_HANDLE;
}

UiHandle UiDragDropManager::resolveTarget(const UiDocument& document,
                                          const UiModalManager& modalManager,
                                          UiHandle handle,
                                          const UiDragPayload& payload) const
{
    UiHandle current = handle;
    while (current != UI_INVALID_HANDLE)
    {
        if (!sameHandle(current, context.source))
        {
            const auto it = targets.find(current);
            if (it != targets.end() &&
                it->second.desc.enabled &&
                targetAcceptsPayload(it->second.desc, payload) &&
                isNodeVisibleAndEnabled(document, current) &&
                isLayerAvailableForPointer(document, modalManager, current))
            {
                return current;
            }
        }

        const UiNode* node = document.findNode(current);
        if (node == nullptr)
            return UI_INVALID_HANDLE;
        current = node->parent;
    }
    return UI_INVALID_HANDLE;
}

bool UiDragDropManager::isNodeVisibleAndEnabled(const UiDocument& document, UiHandle handle) const
{
    const UiNode* node = document.findNode(handle);
    while (node != nullptr)
    {
        if (!node->state.visible || !node->state.enabled)
            return false;
        if (node->parent == UI_INVALID_HANDLE)
            return true;
        node = document.findNode(node->parent);
    }
    return false;
}

bool UiDragDropManager::isLayerAvailableForPointer(const UiDocument& document,
                                                   const UiModalManager& modalManager,
                                                   UiHandle handle) const
{
    const UiLayerId layer = document.getNodeLayer(handle);
    return !modalManager.isLayerBlockedForPointer(document, layer);
}

bool UiDragDropManager::targetAcceptsPayload(const UiDropTargetDesc& desc, const UiDragPayload& payload) const
{
    if (desc.acceptsAnyPayload)
        return true;

    if (payload.type.empty())
        return false;

    return std::find(desc.acceptedPayloadTypes.begin(),
                     desc.acceptedPayloadTypes.end(),
                     payload.type) != desc.acceptedPayloadTypes.end();
}

bool UiDragDropManager::shouldStartDrag(const UiInputFrame& input) const
{
    const auto it = sources.find(context.source);
    if (it == sources.end())
        return false;

    const float dx = input.pointerX - context.startX;
    const float dy = input.pointerY - context.startY;
    const float threshold = std::max(0.0f, it->second.desc.startThreshold);
    return dx * dx + dy * dy >= threshold * threshold;
}

UiDragFrameResult UiDragDropManager::snapshotResult(bool includeActive) const
{
    UiDragFrameResult result;
    if (!includeActive || context.state == UiDragState::Idle)
        return result;

    result.active = true;
    result.armed = context.state == UiDragState::Armed;
    result.dragging = context.state == UiDragState::Dragging;
    result.pointerId = context.pointerId;
    result.source = context.source;
    result.target = context.target;
    result.payload = context.payload;
    result.startX = context.startX;
    result.startY = context.startY;
    result.pointerX = context.pointerX;
    result.pointerY = context.pointerY;
    result.deltaX = context.pointerX - context.startX;
    result.deltaY = context.pointerY - context.startY;
    return result;
}

UiDragFrameResult UiDragDropManager::cancel(UiDocument& /*document*/,
                                            UiFocusManager& focusManager,
                                            bool markCancelled,
                                            UiPointerId pointerId)
{
    UiDragFrameResult result = snapshotResult(true);
    result.cancelled = markCancelled && context.state == UiDragState::Dragging;
    result.ended = context.state == UiDragState::Dragging;
    clearContext(focusManager, pointerId);
    return result;
}

void UiDragDropManager::clearContext(UiFocusManager& focusManager, UiPointerId pointerId)
{
    focusManager.releasePointer(pointerId);
    context = {};
}
