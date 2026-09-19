#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "UiDocument.h"
#include "UiFocusManager.h"
#include "UiLayer.h"
#include "UiModalManager.h"
#include "UiMountTypes.h"

struct UiMountAttachment
{
    UiLayerId layer = UI_INVALID_LAYER;
    UiMountId parentMount = UI_INVALID_MOUNT;
    UiHandle parentNode = UI_INVALID_HANDLE;
};

struct UiMountDesc
{
    std::string name;
    UiMountAttachment attachment;
};

struct UiMount
{
    UiMountId id = UI_INVALID_MOUNT;
    std::string name;
    UiHandle root = UI_INVALID_HANDLE;
    UiLayerId layer = UI_INVALID_LAYER;
    UiMountId parentMount = UI_INVALID_MOUNT;
    UiHandle parentNode = UI_INVALID_HANDLE;
    std::vector<UiMountId> childMounts;
};

class UiMountManager
{
public:
    void reset(UiDocument& document);
    void clear();

    UiMountId createMount(UiDocument& document, const UiMountDesc& desc = {});
    bool destroyMount(UiDocument& document,
                      UiFocusManager& focusManager,
                      UiModalManager& modalManager,
                      UiMountId mountId);
    bool attachMount(UiDocument& document, UiMountId mountId, const UiMountAttachment& attachment);
    bool detachMount(UiDocument& document, UiMountId mountId);

    void pruneInvalidMounts(const UiDocument& document);
    void destroyMountsInLayer(UiDocument& document,
                              UiFocusManager& focusManager,
                              UiModalManager& modalManager,
                              UiLayerId layerId);

    UiMountId rootMount() const { return UI_ROOT_MOUNT; }
    UiMount* findMount(UiMountId mountId);
    const UiMount* findMount(UiMountId mountId) const;
    UiMountId mountFromNode(const UiDocument& document, UiHandle handle) const;
    UiMountId mountFromRoot(UiHandle root) const;
    UiHandle rootForMount(UiMountId mountId) const;
    std::vector<UiMountId> mountsInLayer(UiLayerId layerId) const;

private:
    bool resolveAttachment(const UiDocument& document,
                           UiMountId mountingId,
                           const UiMountAttachment& requested,
                           UiMountAttachment& resolved) const;
    void detachFromParent(UiMountId mountId);
    void addChild(UiMountId parentId, UiMountId childId);
    void updateLayerRecursive(const UiDocument& document, UiMountId mountId);

    std::unordered_map<UiMountId, UiMount> mounts;
    std::unordered_map<UiHandle, UiMountId> rootToMount;
    UiMountId nextMountId = UI_ROOT_MOUNT + 1;
};
