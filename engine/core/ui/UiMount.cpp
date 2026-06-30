#include "UiMount.h"

#include <algorithm>

void UiMountManager::reset(UiDocument& document)
{
    clear();

    UiMount root;
    root.id = UI_ROOT_MOUNT;
    root.name = "root";
    root.root = document.getDocumentRoot();
    root.layer = UI_INVALID_LAYER;
    root.parentMount = UI_INVALID_MOUNT;
    root.parentNode = UI_INVALID_HANDLE;
    mounts.emplace(root.id, root);
    rootToMount[root.root] = root.id;
}

void UiMountManager::clear()
{
    mounts.clear();
    rootToMount.clear();
    nextMountId = UI_ROOT_MOUNT + 1;
}

UiMountId UiMountManager::createMount(UiDocument& document, const UiMountDesc& desc)
{
    UiMountAttachment attachment;
    if (!resolveAttachment(document, UI_INVALID_MOUNT, desc.attachment, attachment))
        return UI_INVALID_MOUNT;

    const UiHandle root = document.createNode(UiNodeType::Container, attachment.parentNode);
    if (root == UI_INVALID_HANDLE)
        return UI_INVALID_MOUNT;

    UiLayoutSpec layout;
    layout.positionMode = UiPositionMode::Anchored;
    layout.widthMode = UiSizeMode::FromAnchors;
    layout.heightMode = UiSizeMode::FromAnchors;
    layout.anchorMin = {0.0f, 0.0f};
    layout.anchorMax = {1.0f, 1.0f};
    document.setLayout(root, layout);
    document.setState(root, UiStateFlags{.visible = true, .enabled = true, .interactable = false});

    UiMount mount;
    mount.id = nextMountId++;
    mount.name = desc.name;
    mount.root = root;
    mount.layer = attachment.layer;
    mount.parentMount = attachment.parentMount;
    mount.parentNode = attachment.parentNode;

    const UiMountId mountId = mount.id;
    mounts.emplace(mountId, mount);
    rootToMount[root] = mountId;
    addChild(attachment.parentMount, mountId);
    return mountId;
}

bool UiMountManager::destroyMount(UiDocument& document,
                                  UiFocusManager& focusManager,
                                  UiModalManager& modalManager,
                                  UiMountId mountId)
{
    auto it = mounts.find(mountId);
    if (it == mounts.end() || mountId == UI_ROOT_MOUNT)
        return false;

    const UiMount mount = it->second;
    const std::vector<UiMountId> children = mount.childMounts;
    for (UiMountId child : children)
        destroyMount(document, focusManager, modalManager, child);

    modalManager.dismissModalsOwnedByMount(document, focusManager, mountId);
    modalManager.dismissModalsForSubtree(document, focusManager, mount.root);
    focusManager.clearForSubtree(document, mount.root);

    detachFromParent(mountId);
    rootToMount.erase(mount.root);
    mounts.erase(mountId);

    if (document.findNode(mount.root) != nullptr)
        document.removeNode(mount.root);

    return true;
}

bool UiMountManager::attachMount(UiDocument& document, UiMountId mountId, const UiMountAttachment& requested)
{
    auto it = mounts.find(mountId);
    if (it == mounts.end() || mountId == UI_ROOT_MOUNT)
        return false;

    UiMountAttachment attachment;
    if (!resolveAttachment(document, mountId, requested, attachment))
        return false;

    UiMount& mount = it->second;
    if (attachment.parentNode == mount.parentNode && attachment.parentMount == mount.parentMount)
        return true;

    if (!document.reparentNode(mount.root, attachment.parentNode))
        return false;

    detachFromParent(mountId);
    mount.parentMount = attachment.parentMount;
    mount.parentNode = attachment.parentNode;
    addChild(mount.parentMount, mountId);
    updateLayerRecursive(document, mountId);
    return true;
}

bool UiMountManager::detachMount(UiDocument& document, UiMountId mountId)
{
    UiMountAttachment attachment;
    attachment.layer = document.getDefaultLayer();
    attachment.parentMount = UI_ROOT_MOUNT;
    return attachMount(document, mountId, attachment);
}

void UiMountManager::pruneInvalidMounts(const UiDocument& document)
{
    for (auto it = mounts.begin(); it != mounts.end();)
    {
        if (it->first != UI_ROOT_MOUNT && document.findNode(it->second.root) == nullptr)
        {
            detachFromParent(it->first);
            rootToMount.erase(it->second.root);
            it = mounts.erase(it);
            continue;
        }

        ++it;
    }
}

void UiMountManager::destroyMountsInLayer(UiDocument& document,
                                          UiFocusManager& focusManager,
                                          UiModalManager& modalManager,
                                          UiLayerId layerId)
{
    bool destroyed = true;
    while (destroyed)
    {
        destroyed = false;
        for (const auto& [mountId, mount] : mounts)
        {
            if (mountId != UI_ROOT_MOUNT && mount.layer == layerId)
            {
                destroyMount(document, focusManager, modalManager, mountId);
                destroyed = true;
                break;
            }
        }
    }
}

UiMount* UiMountManager::findMount(UiMountId mountId)
{
    const auto it = mounts.find(mountId);
    return it == mounts.end() ? nullptr : &it->second;
}

const UiMount* UiMountManager::findMount(UiMountId mountId) const
{
    const auto it = mounts.find(mountId);
    return it == mounts.end() ? nullptr : &it->second;
}

UiMountId UiMountManager::mountFromNode(const UiDocument& document, UiHandle handle) const
{
    const UiNode* node = document.findNode(handle);
    while (node != nullptr)
    {
        const auto it = rootToMount.find(node->handle);
        if (it != rootToMount.end())
            return it->second;

        if (node->parent == UI_INVALID_HANDLE)
            break;
        node = document.findNode(node->parent);
    }

    return UI_INVALID_MOUNT;
}

UiMountId UiMountManager::mountFromRoot(UiHandle root) const
{
    const auto it = rootToMount.find(root);
    return it == rootToMount.end() ? UI_INVALID_MOUNT : it->second;
}

UiHandle UiMountManager::rootForMount(UiMountId mountId) const
{
    const UiMount* mount = findMount(mountId);
    return mount == nullptr ? UI_INVALID_HANDLE : mount->root;
}

bool UiMountManager::resolveAttachment(const UiDocument& document,
                                       UiMountId mountingId,
                                       const UiMountAttachment& requested,
                                       UiMountAttachment& resolved) const
{
    resolved = requested;

    if (resolved.parentNode != UI_INVALID_HANDLE)
    {
        if (document.findNode(resolved.parentNode) == nullptr)
            return false;

        if (mountingId != UI_INVALID_MOUNT)
        {
            const UiMount* mounting = findMount(mountingId);
            if (mounting == nullptr || document.isDescendantOf(resolved.parentNode, mounting->root))
                return false;
        }

        if (resolved.parentMount == UI_INVALID_MOUNT)
            resolved.parentMount = mountFromNode(document, resolved.parentNode);
        if (resolved.parentMount == UI_INVALID_MOUNT)
            resolved.parentMount = UI_ROOT_MOUNT;

        resolved.layer = document.getNodeLayer(resolved.parentNode);
        return resolved.layer != UI_INVALID_LAYER;
    }

    if (resolved.parentMount != UI_INVALID_MOUNT && resolved.parentMount != UI_ROOT_MOUNT)
    {
        const UiMount* parent = findMount(resolved.parentMount);
        if (parent == nullptr || document.findNode(parent->root) == nullptr)
            return false;

        if (mountingId != UI_INVALID_MOUNT)
        {
            const UiMount* mounting = findMount(mountingId);
            if (mounting == nullptr || document.isDescendantOf(parent->root, mounting->root))
                return false;
        }

        resolved.parentNode = parent->root;
        resolved.layer = parent->layer;
        return resolved.layer != UI_INVALID_LAYER;
    }

    resolved.parentMount = UI_ROOT_MOUNT;
    if (resolved.layer == UI_INVALID_LAYER)
        resolved.layer = document.getDefaultLayer();

    resolved.parentNode = document.getLayerRoot(resolved.layer);
    return resolved.parentNode != UI_INVALID_HANDLE;
}

void UiMountManager::detachFromParent(UiMountId mountId)
{
    UiMount* mount = findMount(mountId);
    if (mount == nullptr || mount->parentMount == UI_INVALID_MOUNT)
        return;

    UiMount* parent = findMount(mount->parentMount);
    if (parent != nullptr)
    {
        auto& children = parent->childMounts;
        children.erase(std::remove(children.begin(), children.end(), mountId), children.end());
    }
}

void UiMountManager::addChild(UiMountId parentId, UiMountId childId)
{
    UiMount* parent = findMount(parentId);
    if (parent == nullptr)
        return;

    auto& children = parent->childMounts;
    if (std::find(children.begin(), children.end(), childId) == children.end())
        children.push_back(childId);
}

void UiMountManager::updateLayerRecursive(const UiDocument& document, UiMountId mountId)
{
    UiMount* mount = findMount(mountId);
    if (mount == nullptr)
        return;

    mount->layer = document.getNodeLayer(mount->root);
    const std::vector<UiMountId> children = mount->childMounts;
    for (UiMountId child : children)
        updateLayerRecursive(document, child);
}
