#include "UiNavigationGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr float EPSILON = 0.0001f;

    int tabSortKey(const UiNavigationNode& node)
    {
        return node.desc.tabIndex >= 0
            ? node.desc.tabIndex
            : std::numeric_limits<int>::max();
    }

    bool sameHandle(UiHandle lhs, UiHandle rhs)
    {
        return lhs != UI_INVALID_HANDLE && lhs == rhs;
    }
}

void UiNavigationGraph::clear()
{
    nodes.clear();
    nextRegistrationOrder = 1;
}

bool UiNavigationGraph::registerNode(UiHandle handle, const UiNavigationNodeDesc& desc)
{
    if (handle == UI_INVALID_HANDLE)
        return false;

    UiNavigationNode node;
    node.handle = handle;
    node.desc = desc;

    const auto it = nodes.find(handle);
    if (it != nodes.end())
        node.registrationOrder = it->second.registrationOrder;
    else
        node.registrationOrder = nextRegistrationOrder++;

    nodes[handle] = node;
    return true;
}

bool UiNavigationGraph::unregisterNode(UiHandle handle)
{
    return nodes.erase(handle) > 0;
}

void UiNavigationGraph::unregisterSubtree(const UiDocument& document, UiHandle root)
{
    if (root == UI_INVALID_HANDLE)
        return;

    for (auto it = nodes.begin(); it != nodes.end();)
    {
        if (it->first == root || document.isDescendantOf(it->first, root))
            it = nodes.erase(it);
        else
            ++it;
    }
}

void UiNavigationGraph::pruneInvalidNodes(const UiDocument& document)
{
    for (auto it = nodes.begin(); it != nodes.end();)
    {
        if (document.findNode(it->first) == nullptr)
            it = nodes.erase(it);
        else
            ++it;
    }
}

bool UiNavigationGraph::setNeighbor(UiHandle handle, UiNavigationDirection direction, UiHandle neighbor)
{
    const auto it = nodes.find(handle);
    const size_t index = uiNavigationDirectionIndex(direction);
    if (it == nodes.end() || index >= UI_NAVIGATION_DIRECTION_COUNT)
        return false;

    it->second.desc.neighbors[index] = neighbor;
    return true;
}

bool UiNavigationGraph::setNodeEnabled(UiHandle handle, bool enabled)
{
    const auto it = nodes.find(handle);
    if (it == nodes.end())
        return false;

    it->second.desc.enabled = enabled;
    return true;
}

const UiNavigationNode* UiNavigationGraph::findNode(UiHandle handle) const
{
    const auto it = nodes.find(handle);
    return it == nodes.end() ? nullptr : &it->second;
}

std::vector<UiHandle> UiNavigationGraph::navigationOrder(const UiDocument& document,
                                                         const UiFocusManager& focusManager,
                                                         const UiModalManager& modalManager,
                                                         UiHandle current) const
{
    const std::vector<Candidate> ordered = candidates(document, focusManager, modalManager, current);
    std::vector<UiHandle> result;
    result.reserve(ordered.size());
    for (const Candidate& candidate : ordered)
        result.push_back(candidate.handle);
    return result;
}

UiNavigationMoveResult UiNavigationGraph::move(UiDocument& document,
                                               UiFocusManager& focusManager,
                                               const UiModalManager& modalManager,
                                               UiNavigationDirection direction,
                                               UiInputDeviceId deviceId)
{
    UiNavigationMoveResult result;
    result.requested = direction != UiNavigationDirection::None;
    result.direction = direction;
    result.deviceId = deviceId;
    if (!result.requested || deviceId == 0)
        return result;

    pruneInvalidNodes(document);

    const UiHandle current = currentNavigationTarget(document, focusManager, modalManager, deviceId);
    result.from = current;

    UiHandle target = resolveExplicitNeighbor(document, focusManager, modalManager, current, direction);
    bool wrapped = false;
    if (target == UI_INVALID_HANDLE)
    {
        const std::vector<Candidate> ordered = candidates(document, focusManager, modalManager, current);
        if (ordered.empty())
        {
            result.blocked = true;
            return result;
        }

        bool wrap = false;
        if (const UiNavigationNode* currentNode = findNode(current))
            wrap = currentNode->desc.wrapNavigation;

        if (direction == UiNavigationDirection::Next || direction == UiNavigationDirection::Previous)
            target = resolveLinear(ordered, current, direction, wrap, wrapped);
        else
            target = resolveDirectional(ordered, current, direction, wrap, wrapped);
    }

    if (target == UI_INVALID_HANDLE)
    {
        result.blocked = true;
        return result;
    }

    if (!focusManager.setNavigationFocus(document, deviceId, target, UiFocusReason::Navigation))
    {
        result.blocked = true;
        return result;
    }

    if (!focusManager.requestFocus(document, target, UiFocusReason::Navigation))
    {
        result.blocked = true;
        return result;
    }

    result.to = target;
    result.wrapped = wrapped;
    result.moved = target != current;
    return result;
}

UiNavigationSubmitResult UiNavigationGraph::submit(UiDocument& document,
                                                   UiFocusManager& focusManager,
                                                   const UiModalManager& modalManager,
                                                   UiInputDeviceId deviceId)
{
    UiNavigationSubmitResult result;
    result.requested = true;
    result.deviceId = deviceId;
    if (deviceId == 0)
    {
        result.blocked = true;
        return result;
    }

    pruneInvalidNodes(document);
    const UiHandle target = currentNavigationTarget(document, focusManager, modalManager, deviceId);
    const UiNavigationNode* node = findNode(target);
    if (node == nullptr || !node->desc.activateOnSubmit)
    {
        result.blocked = true;
        return result;
    }

    result.target = target;
    result.submitted = true;
    return result;
}

std::vector<UiNavigationGraph::Candidate> UiNavigationGraph::candidates(
    const UiDocument& document,
    const UiFocusManager& focusManager,
    const UiModalManager& modalManager,
    UiHandle current) const
{
    std::string currentGroup;
    if (const UiNavigationNode* currentNode = findNode(current))
        currentGroup = currentNode->desc.group;

    std::vector<Candidate> result;
    result.reserve(nodes.size());
    for (const auto& [handle, node] : nodes)
    {
        if (!isNodeAvailable(document, focusManager, modalManager, node, currentGroup))
            continue;

        const UiNode* uiNode = document.findNode(handle);
        if (uiNode == nullptr)
            continue;

        result.push_back(Candidate{handle, uiNode->computedLayout.bounds, node});
    }

    std::sort(result.begin(),
              result.end(),
              [](const Candidate& lhs, const Candidate& rhs)
              {
                  const int lhsTab = tabSortKey(lhs.node);
                  const int rhsTab = tabSortKey(rhs.node);
                  if (lhsTab != rhsTab)
                      return lhsTab < rhsTab;
                  if (std::fabs(lhs.bounds.y - rhs.bounds.y) > EPSILON)
                      return lhs.bounds.y < rhs.bounds.y;
                  if (std::fabs(lhs.bounds.x - rhs.bounds.x) > EPSILON)
                      return lhs.bounds.x < rhs.bounds.x;
                  return lhs.node.registrationOrder < rhs.node.registrationOrder;
              });

    return result;
}

bool UiNavigationGraph::isNodeAvailable(const UiDocument& document,
                                        const UiFocusManager& focusManager,
                                        const UiModalManager& modalManager,
                                        const UiNavigationNode& node,
                                        const std::string& currentGroup) const
{
    if (!node.desc.focusable || !node.desc.enabled || !isNodeVisibleAndEnabled(document, node.handle))
        return false;

    if (node.desc.scope != 0 && node.desc.scope != focusManager.activeScope())
        return false;

    if (!currentGroup.empty() && node.desc.group != currentGroup)
        return false;

    if (modalManager.hasModal() && modalManager.blocksNavigation())
    {
        const UiHandle owner = modalManager.topOwner();
        if (owner != UI_INVALID_HANDLE)
            return node.handle == owner || document.isDescendantOf(node.handle, owner);

        const UiLayerId layer = modalManager.topLayer();
        if (layer != UI_INVALID_LAYER)
            return document.getNodeLayer(node.handle) == layer;
    }

    return true;
}

bool UiNavigationGraph::isNodeVisibleAndEnabled(const UiDocument& document, UiHandle handle) const
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

UiHandle UiNavigationGraph::currentNavigationTarget(const UiDocument& document,
                                                    const UiFocusManager& focusManager,
                                                    const UiModalManager& modalManager,
                                                    UiInputDeviceId deviceId) const
{
    const UiHandle navigationFocus = focusManager.navigationFocusedNode(deviceId);
    if (const UiNavigationNode* node = findNode(navigationFocus))
    {
        if (isNodeAvailable(document, focusManager, modalManager, *node, node->desc.group))
            return navigationFocus;
    }

    const UiHandle keyboardFocus = focusManager.keyboardFocusedNode();
    if (const UiNavigationNode* node = findNode(keyboardFocus))
    {
        if (isNodeAvailable(document, focusManager, modalManager, *node, node->desc.group))
            return keyboardFocus;
    }

    return UI_INVALID_HANDLE;
}

UiHandle UiNavigationGraph::resolveExplicitNeighbor(const UiDocument& document,
                                                    const UiFocusManager& focusManager,
                                                    const UiModalManager& modalManager,
                                                    UiHandle current,
                                                    UiNavigationDirection direction) const
{
    const UiNavigationNode* currentNode = findNode(current);
    const size_t index = uiNavigationDirectionIndex(direction);
    if (currentNode == nullptr || index >= UI_NAVIGATION_DIRECTION_COUNT)
        return UI_INVALID_HANDLE;

    const UiHandle neighbor = currentNode->desc.neighbors[index];
    const UiNavigationNode* neighborNode = findNode(neighbor);
    if (neighborNode == nullptr)
        return UI_INVALID_HANDLE;

    return isNodeAvailable(document,
                           focusManager,
                           modalManager,
                           *neighborNode,
                           currentNode->desc.group)
        ? neighbor
        : UI_INVALID_HANDLE;
}

UiHandle UiNavigationGraph::resolveLinear(const std::vector<Candidate>& candidates,
                                          UiHandle current,
                                          UiNavigationDirection direction,
                                          bool wrap,
                                          bool& wrapped) const
{
    if (candidates.empty())
        return UI_INVALID_HANDLE;

    if (current == UI_INVALID_HANDLE)
        return direction == UiNavigationDirection::Previous ? candidates.back().handle : candidates.front().handle;

    const auto it = std::find_if(candidates.begin(),
                                 candidates.end(),
                                 [&](const Candidate& candidate)
                                 {
                                     return candidate.handle == current;
                                 });
    if (it == candidates.end())
        return direction == UiNavigationDirection::Previous ? candidates.back().handle : candidates.front().handle;

    if (direction == UiNavigationDirection::Previous)
    {
        if (it != candidates.begin())
            return std::prev(it)->handle;
        if (wrap)
        {
            wrapped = true;
            return candidates.back().handle;
        }
        return UI_INVALID_HANDLE;
    }

    const auto next = std::next(it);
    if (next != candidates.end())
        return next->handle;
    if (wrap)
    {
        wrapped = true;
        return candidates.front().handle;
    }
    return UI_INVALID_HANDLE;
}

UiHandle UiNavigationGraph::resolveDirectional(const std::vector<Candidate>& candidates,
                                               UiHandle current,
                                               UiNavigationDirection direction,
                                               bool wrap,
                                               bool& wrapped) const
{
    if (candidates.empty())
        return UI_INVALID_HANDLE;
    if (current == UI_INVALID_HANDLE)
        return isForwardDirection(direction) ? candidates.front().handle : candidates.back().handle;

    const auto currentIt = std::find_if(candidates.begin(),
                                        candidates.end(),
                                        [&](const Candidate& candidate)
                                        {
                                            return candidate.handle == current;
                                        });
    if (currentIt == candidates.end())
        return isForwardDirection(direction) ? candidates.front().handle : candidates.back().handle;

    const float cx = centerX(currentIt->bounds);
    const float cy = centerY(currentIt->bounds);
    UiHandle best = UI_INVALID_HANDLE;
    float bestScore = std::numeric_limits<float>::max();

    for (const Candidate& candidate : candidates)
    {
        if (sameHandle(candidate.handle, current))
            continue;

        const float dx = centerX(candidate.bounds) - cx;
        const float dy = centerY(candidate.bounds) - cy;

        float major = 0.0f;
        float cross = 0.0f;
        switch (direction)
        {
            case UiNavigationDirection::Up:
                if (dy >= -EPSILON)
                    continue;
                major = -dy;
                cross = std::fabs(dx);
                break;
            case UiNavigationDirection::Down:
                if (dy <= EPSILON)
                    continue;
                major = dy;
                cross = std::fabs(dx);
                break;
            case UiNavigationDirection::Left:
                if (dx >= -EPSILON)
                    continue;
                major = -dx;
                cross = std::fabs(dy);
                break;
            case UiNavigationDirection::Right:
                if (dx <= EPSILON)
                    continue;
                major = dx;
                cross = std::fabs(dy);
                break;
            default:
                continue;
        }

        const float score = major * 4.0f + cross;
        if (score < bestScore)
        {
            bestScore = score;
            best = candidate.handle;
        }
    }

    if (best != UI_INVALID_HANDLE || !wrap)
        return best;

    wrapped = true;
    UiHandle wrapTarget = UI_INVALID_HANDLE;
    float wrapScore = std::numeric_limits<float>::max();
    for (const Candidate& candidate : candidates)
    {
        if (sameHandle(candidate.handle, current))
            continue;

        float major = 0.0f;
        float cross = 0.0f;
        switch (direction)
        {
            case UiNavigationDirection::Up:
                major = -centerY(candidate.bounds);
                cross = std::fabs(centerX(candidate.bounds) - cx);
                break;
            case UiNavigationDirection::Down:
                major = centerY(candidate.bounds);
                cross = std::fabs(centerX(candidate.bounds) - cx);
                break;
            case UiNavigationDirection::Left:
                major = -centerX(candidate.bounds);
                cross = std::fabs(centerY(candidate.bounds) - cy);
                break;
            case UiNavigationDirection::Right:
                major = centerX(candidate.bounds);
                cross = std::fabs(centerY(candidate.bounds) - cy);
                break;
            default:
                continue;
        }

        const float score = major * 4.0f + cross;
        if (score < wrapScore)
        {
            wrapScore = score;
            wrapTarget = candidate.handle;
        }
    }

    return wrapTarget;
}

bool UiNavigationGraph::isForwardDirection(UiNavigationDirection direction)
{
    return direction == UiNavigationDirection::Down ||
           direction == UiNavigationDirection::Right ||
           direction == UiNavigationDirection::Next;
}

float UiNavigationGraph::centerX(const UiRect& rect)
{
    return rect.x + rect.width * 0.5f;
}

float UiNavigationGraph::centerY(const UiRect& rect)
{
    return rect.y + rect.height * 0.5f;
}
