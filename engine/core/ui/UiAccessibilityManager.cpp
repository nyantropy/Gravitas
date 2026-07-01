#include "UiAccessibilityManager.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

namespace
{
    float clamp01(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    bool semanticVectorContains(const std::vector<UiHandle>& values, UiHandle handle)
    {
        return std::find(values.begin(), values.end(), handle) != values.end();
    }
}

void UiAccessibilityManager::clear()
{
    semantics.clear();
    lastSnapshots.clear();
    announcementQueue.clear();
    policyState = {};
    lastFocused = UI_INVALID_HANDLE;
    nextAnnouncementSequence = 1;
}

bool UiAccessibilityManager::setSemantic(UiHandle handle, UiSemanticDesc desc)
{
    if (handle == UI_INVALID_HANDLE)
        return false;

    semantics[handle] = std::move(desc);
    return true;
}

bool UiAccessibilityManager::clearSemantic(UiHandle handle)
{
    lastSnapshots.erase(handle);
    if (lastFocused == handle)
        lastFocused = UI_INVALID_HANDLE;
    return semantics.erase(handle) > 0;
}

uint32_t UiAccessibilityManager::clearSubtree(const UiDocument& document, UiHandle root)
{
    if (root == UI_INVALID_HANDLE)
        return 0;

    uint32_t removed = 0;
    for (auto it = semantics.begin(); it != semantics.end();)
    {
        if (it->first == root || document.isDescendantOf(it->first, root))
        {
            lastSnapshots.erase(it->first);
            if (lastFocused == it->first)
                lastFocused = UI_INVALID_HANDLE;
            it = semantics.erase(it);
            ++removed;
        }
        else
        {
            ++it;
        }
    }
    return removed;
}

void UiAccessibilityManager::pruneInvalidNodes(const UiDocument& document)
{
    for (auto it = semantics.begin(); it != semantics.end();)
    {
        if (document.findNode(it->first) == nullptr)
        {
            lastSnapshots.erase(it->first);
            if (lastFocused == it->first)
                lastFocused = UI_INVALID_HANDLE;
            it = semantics.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool UiAccessibilityManager::hasSemantic(UiHandle handle) const
{
    return semantics.find(handle) != semantics.end();
}

const UiSemanticDesc* UiAccessibilityManager::semanticDesc(UiHandle handle) const
{
    const auto it = semantics.find(handle);
    return it == semantics.end() ? nullptr : &it->second;
}

UiSemanticTree UiAccessibilityManager::snapshotTree(const UiDocument& document) const
{
    UiSemanticTree tree;
    buildTreeRecursive(document, document.getDocumentRoot(), UI_INVALID_HANDLE, true, true, tree);
    return tree;
}

UiAccessibilityFrameResult UiAccessibilityManager::update(const UiDocument& document)
{
    const size_t before = semantics.size();
    pruneInvalidNodes(document);

    UiAccessibilityFrameResult result;
    result.pruned = static_cast<uint32_t>(before - semantics.size());

    const UiSemanticTree tree = snapshotTree(document);
    result.semanticNodes = static_cast<uint32_t>(tree.nodes.size());

    std::unordered_map<UiHandle, Snapshot> current;
    current.reserve(tree.nodes.size());
    UiHandle focused = UI_INVALID_HANDLE;
    std::string focusedName;
    UiSemanticRole focusedRole = UiSemanticRole::Unknown;

    for (const UiSemanticNode& node : tree.nodes)
    {
        Snapshot snapshot = makeSnapshot(node);
        if (node.focused)
        {
            focused = node.handle;
            focusedName = node.name;
            focusedRole = node.role;
        }

        const auto previous = lastSnapshots.find(node.handle);
        if (previous != lastSnapshots.end())
        {
            const Snapshot& old = previous->second;
            const bool valueChanged = old.value != snapshot.value ||
                                      old.name != snapshot.name ||
                                      old.hasRange != snapshot.hasRange ||
                                      (snapshot.hasRange && std::fabs(old.rangeValue - snapshot.rangeValue) > 0.0001f);
            const bool stateChanged = old.enabled != snapshot.enabled ||
                                      old.hidden != snapshot.hidden ||
                                      old.selected != snapshot.selected ||
                                      old.checked != snapshot.checked ||
                                      old.expanded != snapshot.expanded ||
                                      old.busy != snapshot.busy;

            if (snapshot.liveRegion != UiAccessibilityLiveRegion::Off && valueChanged)
            {
                enqueueAnnouncement(node.handle,
                                    UiAccessibilityAnnouncementKind::LiveRegion,
                                    node.role,
                                    snapshot.liveRegion,
                                    !snapshot.value.empty() ? snapshot.value : snapshot.name);
            }
            else if (snapshot.focused && valueChanged)
            {
                enqueueAnnouncement(node.handle,
                                    UiAccessibilityAnnouncementKind::ValueChanged,
                                    node.role,
                                    UiAccessibilityLiveRegion::Polite,
                                    !snapshot.value.empty() ? snapshot.value : snapshot.name);
            }
            else if (snapshot.focused && stateChanged)
            {
                enqueueAnnouncement(node.handle,
                                    UiAccessibilityAnnouncementKind::StateChanged,
                                    node.role,
                                    UiAccessibilityLiveRegion::Polite,
                                    snapshot.name);
            }
        }

        current[node.handle] = std::move(snapshot);
    }

    if (focused != UI_INVALID_HANDLE && focused != lastFocused)
    {
        enqueueAnnouncement(focused,
                            UiAccessibilityAnnouncementKind::FocusChanged,
                            focusedRole,
                            UiAccessibilityLiveRegion::Polite,
                            focusedName);
    }

    lastFocused = focused;
    lastSnapshots = std::move(current);
    result.announcements = static_cast<uint32_t>(announcementQueue.size());
    return result;
}

bool UiAccessibilityManager::applyBindingValue(const UiDocument& document,
                                               UiHandle target,
                                               UiBindableProperty property,
                                               const UiBindingValue& value)
{
    auto it = semantics.find(target);
    if (it == semantics.end() || document.findNode(target) == nullptr)
        return false;

    UiSemanticDesc& semantic = it->second;
    switch (property)
    {
        case UiBindableProperty::Text:
        case UiBindableProperty::StyleClass:
        case UiBindableProperty::ImageAsset:
        {
            const std::string text = uiBindingValueToString(value);
            semantic.value = text;
            return true;
        }

        case UiBindableProperty::Visible:
        {
            bool visible = true;
            if (!uiBindingValueAsBool(value, visible))
                return false;
            semantic.hidden = !visible;
            return true;
        }

        case UiBindableProperty::Enabled:
        case UiBindableProperty::Interactable:
            return true;

        case UiBindableProperty::Progress:
        {
            float progress = 0.0f;
            if (!uiBindingValueAsFloat(value, progress))
                return false;
            semantic.hasRange = true;
            semantic.rangeMin = 0.0f;
            semantic.rangeMax = 1.0f;
            semantic.rangeValue = clamp01(progress);
            semantic.value = uiBindingValueToString(value);
            return true;
        }

        case UiBindableProperty::Opacity:
        case UiBindableProperty::RectColor:
        case UiBindableProperty::TextColor:
        case UiBindableProperty::ImageTint:
        case UiBindableProperty::Layout:
        case UiBindableProperty::LayoutOffsetMin:
        case UiBindableProperty::LayoutOffsetMax:
        case UiBindableProperty::LayoutAnchorMin:
        case UiBindableProperty::LayoutAnchorMax:
        case UiBindableProperty::LayoutFixedSize:
        case UiBindableProperty::LayoutContentOffset:
            return false;
    }

    return false;
}

void UiAccessibilityManager::announce(UiHandle source,
                                      UiAccessibilityAnnouncementKind kind,
                                      std::string text,
                                      UiAccessibilityLiveRegion liveRegion)
{
    UiSemanticRole role = UiSemanticRole::Unknown;
    if (const auto it = semantics.find(source); it != semantics.end())
        role = it->second.role;
    enqueueAnnouncement(source, kind, role, liveRegion, std::move(text));
}

std::vector<UiAccessibilityAnnouncement> UiAccessibilityManager::drainAnnouncements()
{
    std::vector<UiAccessibilityAnnouncement> result = std::move(announcementQueue);
    announcementQueue.clear();
    return result;
}

std::vector<UiHandle> UiAccessibilityManager::findByRoleAndName(const UiDocument& document,
                                                                UiSemanticRole role,
                                                                const std::string& name) const
{
    std::vector<UiHandle> result;
    for (const UiSemanticNode& node : snapshotTree(document).nodes)
    {
        if ((role == UiSemanticRole::Unknown || node.role == role) && node.name == name)
            result.push_back(node.handle);
    }
    std::sort(result.begin(), result.end());
    return result;
}

UiSemanticNode UiAccessibilityManager::makeSemanticNode(const UiDocument& document,
                                                        UiHandle handle,
                                                        const UiSemanticDesc& desc,
                                                        UiHandle parent,
                                                        bool ancestorVisible,
                                                        bool ancestorEnabled) const
{
    const UiNode* node = document.findNode(handle);
    UiSemanticNode semantic;
    semantic.handle = handle;
    semantic.parent = parent;
    semantic.role = desc.role;
    semantic.name = resolveName(document, handle, desc);
    semantic.description = desc.description;
    semantic.hint = desc.hint;
    semantic.value = resolveValue(document, handle, desc);
    semantic.liveRegion = desc.liveRegion;
    semantic.relationships = desc.relationships;
    semantic.ownerMount = desc.ownerMount;
    semantic.bounds = node == nullptr ? UiRect{} : node->computedLayout.bounds;
    semantic.hidden = desc.hidden || !ancestorVisible || node == nullptr || !node->state.visible;
    semantic.enabled = ancestorEnabled && node != nullptr && node->state.enabled;
    semantic.focused = node != nullptr && node->state.focused;
    semantic.selected = desc.selected;
    semantic.checked = desc.checked;
    semantic.expanded = desc.expanded;
    semantic.readOnly = desc.readOnly;
    semantic.busy = desc.busy;
    semantic.hasRange = desc.hasRange;
    semantic.rangeMin = desc.rangeMin;
    semantic.rangeMax = desc.rangeMax;
    semantic.rangeValue = desc.rangeValue;
    semantic.level = desc.level;
    semantic.index = desc.index;
    semantic.count = desc.count;
    return semantic;
}

void UiAccessibilityManager::buildTreeRecursive(const UiDocument& document,
                                                UiHandle handle,
                                                UiHandle semanticParent,
                                                bool ancestorVisible,
                                                bool ancestorEnabled,
                                                UiSemanticTree& tree) const
{
    const UiNode* node = document.findNode(handle);
    if (node == nullptr)
        return;

    const bool currentVisible = ancestorVisible && node->state.visible;
    const bool currentEnabled = ancestorEnabled && node->state.enabled;
    UiHandle currentSemanticParent = semanticParent;

    const auto semanticIt = semantics.find(handle);
    if (semanticIt != semantics.end() &&
        !semanticIt->second.decorative &&
        !semanticIt->second.hidden &&
        currentVisible)
    {
        UiSemanticNode semantic = makeSemanticNode(document,
                                                  handle,
                                                  semanticIt->second,
                                                  semanticParent,
                                                  ancestorVisible,
                                                  ancestorEnabled);
        tree.nodes.push_back(std::move(semantic));
        currentSemanticParent = handle;
        if (semanticParent != UI_INVALID_HANDLE)
        {
            for (UiSemanticNode& candidate : tree.nodes)
            {
                if (candidate.handle == semanticParent &&
                    !semanticVectorContains(candidate.children, handle))
                {
                    candidate.children.push_back(handle);
                    break;
                }
            }
        }
    }

    for (UiHandle child : node->children)
        buildTreeRecursive(document, child, currentSemanticParent, currentVisible, currentEnabled, tree);
}

UiAccessibilityManager::Snapshot UiAccessibilityManager::makeSnapshot(const UiSemanticNode& node) const
{
    Snapshot snapshot;
    snapshot.name = node.name;
    snapshot.value = node.value;
    snapshot.role = node.role;
    snapshot.liveRegion = node.liveRegion;
    snapshot.focused = node.focused;
    snapshot.enabled = node.enabled;
    snapshot.hidden = node.hidden;
    snapshot.selected = node.selected;
    snapshot.checked = node.checked;
    snapshot.expanded = node.expanded;
    snapshot.busy = node.busy;
    snapshot.hasRange = node.hasRange;
    snapshot.rangeValue = node.rangeValue;
    return snapshot;
}

std::string UiAccessibilityManager::resolveName(const UiDocument& document,
                                                UiHandle handle,
                                                const UiSemanticDesc& desc) const
{
    if (!desc.name.empty())
        return desc.name;

    std::string name;
    for (UiHandle label : desc.relationships.labelledBy)
    {
        const std::string text = textForHandle(document, label);
        if (text.empty())
            continue;
        if (!name.empty())
            name += " ";
        name += text;
    }
    if (!name.empty())
        return name;

    const std::string ownText = textForHandle(document, handle);
    if (!ownText.empty())
        return ownText;

    return {};
}

std::string UiAccessibilityManager::resolveValue(const UiDocument& document,
                                                 UiHandle handle,
                                                 const UiSemanticDesc& desc) const
{
    if (!desc.value.empty())
        return desc.value;

    if (desc.hasRange)
        return std::to_string(desc.rangeValue);

    return textForHandle(document, handle);
}

std::string UiAccessibilityManager::textForHandle(const UiDocument& document, UiHandle handle) const
{
    const UiNode* node = document.findNode(handle);
    if (node == nullptr)
        return {};

    if (const auto* text = std::get_if<UiTextData>(&node->payload))
        return text->text;
    if (const auto* image = std::get_if<UiImageData>(&node->payload))
        return image->imageAsset;
    if (const auto* slice = std::get_if<UiNineSliceData>(&node->payload))
        return slice->imageAsset;
    return {};
}

void UiAccessibilityManager::enqueueAnnouncement(UiHandle source,
                                                 UiAccessibilityAnnouncementKind kind,
                                                 UiSemanticRole role,
                                                 UiAccessibilityLiveRegion liveRegion,
                                                 std::string text)
{
    if (text.empty())
        return;

    UiAccessibilityAnnouncement announcement;
    announcement.sequence = nextAnnouncementSequence++;
    announcement.source = source;
    announcement.kind = kind;
    announcement.role = role;
    announcement.liveRegion = liveRegion;
    announcement.text = std::move(text);
    announcementQueue.push_back(std::move(announcement));
}
