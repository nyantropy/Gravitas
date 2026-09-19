#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "UiAccessibilityTypes.h"
#include "UiBindingTypes.h"
#include "UiDocument.h"

class UiAccessibilityManager
{
public:
    void clear();

    bool setSemantic(UiHandle handle, UiSemanticDesc desc);
    bool clearSemantic(UiHandle handle);
    uint32_t clearSubtree(const UiDocument& document, UiHandle root);
    void pruneInvalidNodes(const UiDocument& document);

    bool hasSemantic(UiHandle handle) const;
    const UiSemanticDesc* semanticDesc(UiHandle handle) const;
    size_t semanticCount() const { return semantics.size(); }

    UiSemanticTree snapshotTree(const UiDocument& document) const;
    UiAccessibilityFrameResult update(const UiDocument& document);

    bool applyBindingValue(const UiDocument& document,
                           UiHandle target,
                           UiBindableProperty property,
                           const UiBindingValue& value);

    void announce(UiHandle source,
                  UiAccessibilityAnnouncementKind kind,
                  std::string text,
                  UiAccessibilityLiveRegion liveRegion = UiAccessibilityLiveRegion::Polite);
    const std::vector<UiAccessibilityAnnouncement>& announcements() const { return announcementQueue; }
    std::vector<UiAccessibilityAnnouncement> drainAnnouncements();

    std::vector<UiHandle> findByRoleAndName(const UiDocument& document,
                                            UiSemanticRole role,
                                            const std::string& name) const;

    void setPolicy(const UiAccessibilityPolicy& inPolicy) { policyState = inPolicy; }
    const UiAccessibilityPolicy& policy() const { return policyState; }

private:
    struct Snapshot
    {
        std::string name;
        std::string value;
        UiSemanticRole role = UiSemanticRole::Unknown;
        UiAccessibilityLiveRegion liveRegion = UiAccessibilityLiveRegion::Off;
        bool focused = false;
        bool enabled = true;
        bool hidden = false;
        bool selected = false;
        bool checked = false;
        bool expanded = false;
        bool busy = false;
        bool hasRange = false;
        float rangeValue = 0.0f;
    };

    UiSemanticNode makeSemanticNode(const UiDocument& document,
                                    UiHandle handle,
                                    const UiSemanticDesc& desc,
                                    UiHandle parent,
                                    bool ancestorVisible,
                                    bool ancestorEnabled) const;
    void buildTreeRecursive(const UiDocument& document,
                            UiHandle handle,
                            UiHandle semanticParent,
                            bool ancestorVisible,
                            bool ancestorEnabled,
                            UiSemanticTree& tree) const;
    Snapshot makeSnapshot(const UiSemanticNode& node) const;
    std::string resolveName(const UiDocument& document,
                            UiHandle handle,
                            const UiSemanticDesc& desc) const;
    std::string resolveValue(const UiDocument& document,
                             UiHandle handle,
                             const UiSemanticDesc& desc) const;
    std::string textForHandle(const UiDocument& document, UiHandle handle) const;
    void enqueueAnnouncement(UiHandle source,
                             UiAccessibilityAnnouncementKind kind,
                             UiSemanticRole role,
                             UiAccessibilityLiveRegion liveRegion,
                             std::string text);

    std::unordered_map<UiHandle, UiSemanticDesc> semantics;
    std::unordered_map<UiHandle, Snapshot> lastSnapshots;
    std::vector<UiAccessibilityAnnouncement> announcementQueue;
    UiAccessibilityPolicy policyState;
    UiHandle lastFocused = UI_INVALID_HANDLE;
    uint64_t nextAnnouncementSequence = 1;
};
