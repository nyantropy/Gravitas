#include <cstdlib>
#include <iostream>
#include <string>

#include "UiSystem.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI mount runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    UiLayoutSpec rectLayout(float x, float y, float width, float height)
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Absolute;
        layout.widthMode = UiSizeMode::Fixed;
        layout.heightMode = UiSizeMode::Fixed;
        layout.offsetMin = {x, y};
        layout.fixedWidth = width;
        layout.fixedHeight = height;
        return layout;
    }

    UiHandle createHitRect(UiSystem& ui, UiHandle parent, const UiLayoutSpec& layout)
    {
        const UiHandle handle = ui.createNode(UiNodeType::Rect, parent);
        ui.setLayout(handle, layout);
        ui.setState(handle, UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        ui.setPayload(handle, UiRectData{UiColor{1.0f, 1.0f, 1.0f, 1.0f}});
        return handle;
    }

    UiMountDesc mountInLayer(UiLayerId layer, const std::string& name = {})
    {
        UiMountDesc desc;
        desc.name = name;
        desc.attachment.layer = layer;
        return desc;
    }

    UiMountDesc childMount(UiMountId parentMount, const std::string& name = {})
    {
        UiMountDesc desc;
        desc.name = name;
        desc.attachment.parentMount = parentMount;
        return desc;
    }

    UiInputFrame pointerFrame(float x, float y)
    {
        UiInputFrame frame;
        frame.pointerX = x;
        frame.pointerY = y;
        return frame;
    }

    void testMountCreation()
    {
        UiSystem ui(nullptr);
        const UiLayerId layer = ui.createLayer("hud", 10);
        const UiMountId mount = ui.createMount(mountInLayer(layer, "hud-root"));

        require(mount != UI_INVALID_MOUNT, "mount creation failed");
        require(mount != ui.rootMount(), "created mount reused root mount id");
        const UiMount* record = ui.findMount(mount);
        require(record != nullptr, "created mount was not registered");
        require(record->name == "hud-root", "mount name was not preserved");
        require(record->layer == layer, "mount layer was not tracked");
        require(record->parentMount == ui.rootMount(), "top-level mount parent was not root mount");
        require(ui.findNode(record->root) != nullptr, "mount root node was not created");
        require(ui.mountFromNode(record->root) == mount, "mount root did not resolve to mount");

        const UiHandle child = createHitRect(ui, record->root, rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        require(ui.mountFromNode(child) == mount, "child node did not resolve to owning mount");
    }

    void testParentNodeAttachmentInfersChildMount()
    {
        UiSystem ui(nullptr);
        const UiMountId parent = ui.createMount();
        const UiHandle slot = ui.createNode(UiNodeType::Container, ui.mountRoot(parent));

        UiMountDesc desc;
        desc.name = "child";
        desc.attachment.parentNode = slot;
        const UiMountId child = ui.createMount(desc);

        require(child != UI_INVALID_MOUNT, "child mount creation failed");
        const UiMount* childRecord = ui.findMount(child);
        require(childRecord != nullptr, "child mount was not registered");
        require(childRecord->parentMount == parent, "parent node attachment did not infer owning mount");
        require(childRecord->parentNode == slot, "parent node attachment was not preserved");
        require(ui.findMount(parent)->childMounts.size() == 1, "parent did not track child mount");
    }

    void testNestedMountDestruction()
    {
        UiSystem ui(nullptr);
        const UiMountId parent = ui.createMount();
        const UiMountId child = ui.createMount(childMount(parent));
        const UiHandle parentRoot = ui.mountRoot(parent);
        const UiHandle childRoot = ui.mountRoot(child);
        const UiHandle parentNode = createHitRect(ui, parentRoot, rectLayout(0.0f, 0.0f, 0.5f, 1.0f));
        const UiHandle childNode = createHitRect(ui, childRoot, rectLayout(0.5f, 0.0f, 0.5f, 1.0f));

        require(ui.destroyMount(parent), "parent mount destroy failed");
        require(ui.findMount(parent) == nullptr, "destroyed parent mount remained registered");
        require(ui.findMount(child) == nullptr, "destroyed child mount remained registered");
        require(ui.findNode(parentRoot) == nullptr, "parent root survived parent destroy");
        require(ui.findNode(childRoot) == nullptr, "child root survived parent destroy");
        require(ui.findNode(parentNode) == nullptr, "parent subtree survived parent destroy");
        require(ui.findNode(childNode) == nullptr, "child subtree survived parent destroy");
    }

    void testDestroyChildLeavesParent()
    {
        UiSystem ui(nullptr);
        const UiMountId parent = ui.createMount();
        const UiMountId child = ui.createMount(childMount(parent));
        const UiHandle parentRoot = ui.mountRoot(parent);
        const UiHandle childRoot = ui.mountRoot(child);

        require(ui.destroyMount(child), "child mount destroy failed");
        require(ui.findMount(parent) != nullptr, "parent mount was destroyed with child");
        require(ui.findNode(parentRoot) != nullptr, "parent root was destroyed with child");
        require(ui.findMount(child) == nullptr, "child mount remained registered");
        require(ui.findNode(childRoot) == nullptr, "child root survived child destroy");
        require(ui.findMount(parent)->childMounts.empty(), "parent retained destroyed child mount id");
    }

    void testFocusAndCaptureCleanup()
    {
        UiSystem ui(nullptr);
        const UiMountId mount = ui.createMount();
        const UiHandle target = createHitRect(ui, ui.mountRoot(mount), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        UiFocusManager& focus = ui.focusManager();
        require(focus.setHovered(ui.getDocument(), UI_PRIMARY_POINTER, target), "hover setup failed");
        require(focus.capturePointer(ui.getDocument(), UI_PRIMARY_POINTER, target), "capture setup failed");
        require(focus.setActivePointer(ui.getDocument(), UI_PRIMARY_POINTER, target), "active pointer setup failed");
        require(focus.requestFocus(ui.getDocument(), target), "keyboard focus setup failed");
        require(focus.requestTextInputFocus(ui.getDocument(), target), "text focus setup failed");
        require(focus.setNavigationFocus(ui.getDocument(), UI_PRIMARY_INPUT_DEVICE, target),
                "navigation focus setup failed");
        const UiFocusScopeId scope = focus.pushScope(target);
        require(focus.findScope(scope) != nullptr, "focus scope setup failed");

        require(ui.destroyMount(mount), "mount destroy failed");
        require(focus.hoveredNode() == UI_INVALID_HANDLE, "hover survived mount destroy");
        require(focus.capturedNode() == UI_INVALID_HANDLE, "capture survived mount destroy");
        require(focus.activePointerNode() == UI_INVALID_HANDLE, "active pointer survived mount destroy");
        require(focus.focusedNode() == UI_INVALID_HANDLE, "keyboard focus survived mount destroy");
        require(focus.textInputFocusedNode() == UI_INVALID_HANDLE, "text focus survived mount destroy");
        require(focus.navigationFocusedNode() == UI_INVALID_HANDLE, "navigation focus survived mount destroy");
        require(focus.findScope(scope) == nullptr, "focus scope survived mount destroy");
    }

    void testModalCleanup()
    {
        UiSystem ui(nullptr);
        const UiLayerId layer = ui.createLayer("modal", 100);
        const UiMountId mount = ui.createMount(mountInLayer(layer, "modal-content"));
        const UiHandle owner = createHitRect(ui, ui.mountRoot(mount), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        UiModalDesc modalDesc;
        modalDesc.layer = layer;
        modalDesc.owner = owner;
        modalDesc.initialFocus = owner;
        const UiModalId modal = ui.pushModal(modalDesc);
        require(modal != UI_INVALID_MODAL, "modal push failed");

        UiDispatchResult result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 1);
        require(result.modalActive, "modal was not active before mount destroy");
        require(result.modalMount == mount, "modal did not infer owner mount");

        require(ui.destroyMount(mount), "modal owner mount destroy failed");
        require(!ui.modalManager().hasModal(), "modal survived owner mount destroy");
        result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 2);
        require(!result.modalActive, "dispatch reported modal after owner mount destroy");
    }

    void testLayerRemovalDestroysMounts()
    {
        UiSystem ui(nullptr);
        const UiLayerId layer = ui.createLayer("overlay", 100);
        const UiMountId mount = ui.createMount(mountInLayer(layer, "overlay-content"));
        const UiHandle root = ui.mountRoot(mount);
        const UiHandle child = createHitRect(ui, root, rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        require(ui.removeLayer(layer), "layer removal failed");
        require(ui.findMount(mount) == nullptr, "layer removal left mount registered");
        require(ui.findNode(root) == nullptr, "layer removal left mount root");
        require(ui.findNode(child) == nullptr, "layer removal left mount child");
    }

    void testUiClearResetsMounts()
    {
        UiSystem ui(nullptr);
        const UiMountId oldMount = ui.createMount();
        const UiHandle oldRoot = ui.mountRoot(oldMount);

        ui.clear();
        require(ui.findMount(oldMount) == nullptr, "clear kept old mount registered");
        require(ui.findNode(oldRoot) == nullptr, "clear kept old mount root");
        require(ui.findMount(ui.rootMount()) != nullptr, "clear did not recreate root mount");

        const UiMountId newMount = ui.createMount();
        require(newMount != UI_INVALID_MOUNT, "mount creation after clear failed");
        require(ui.findMount(newMount) != nullptr, "new mount after clear was not registered");
    }

    void testAttachAndDetachMount()
    {
        UiSystem ui(nullptr);
        const UiLayerId firstLayer = ui.createLayer("first", 10);
        const UiLayerId secondLayer = ui.createLayer("second", 20);
        const UiMountId mount = ui.createMount(mountInLayer(firstLayer, "panel"));
        const UiHandle root = ui.mountRoot(mount);

        UiMountAttachment secondAttachment;
        secondAttachment.layer = secondLayer;
        secondAttachment.parentMount = UI_ROOT_MOUNT;
        require(ui.attachMount(mount, secondAttachment), "attach to second layer failed");
        require(ui.findMount(mount)->layer == secondLayer, "mount layer did not update after attach");
        require(ui.getDocument().getNodeLayer(root) == secondLayer, "root node layer did not update after attach");

        require(ui.detachMount(mount), "detach to default layer failed");
        require(ui.findMount(mount)->layer == ui.getDefaultLayer(), "mount layer did not update after detach");
        require(ui.getDocument().getNodeLayer(root) == ui.getDefaultLayer(), "root node layer did not update after detach");
    }

    void testManualRemoveMountRootDestroysMount()
    {
        UiSystem ui(nullptr);
        const UiMountId mount = ui.createMount();
        const UiHandle root = ui.mountRoot(mount);
        const UiHandle child = createHitRect(ui, root, rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        require(ui.removeNode(root), "manual remove of mount root failed");
        require(ui.findMount(mount) == nullptr, "manual remove left mount registered");
        require(ui.findNode(root) == nullptr, "manual remove left mount root");
        require(ui.findNode(child) == nullptr, "manual remove left mount child");
    }

    void testInvalidMounts()
    {
        UiSystem ui(nullptr);
        UiMountAttachment attachment;
        attachment.layer = ui.getDefaultLayer();

        require(!ui.destroyMount(UI_INVALID_MOUNT), "invalid mount destroy succeeded");
        require(!ui.destroyMount(ui.rootMount()), "root mount destroy succeeded");
        require(!ui.attachMount(UI_INVALID_MOUNT, attachment), "invalid mount attach succeeded");
        require(!ui.detachMount(UI_INVALID_MOUNT), "invalid mount detach succeeded");
        require(ui.findMount(UI_INVALID_MOUNT) == nullptr, "invalid mount lookup returned record");
        require(ui.mountRoot(UI_INVALID_MOUNT) == UI_INVALID_HANDLE, "invalid mount root lookup returned handle");
    }
}

int main()
{
    testMountCreation();
    testParentNodeAttachmentInfersChildMount();
    testNestedMountDestruction();
    testDestroyChildLeavesParent();
    testFocusAndCaptureCleanup();
    testModalCleanup();
    testLayerRemovalDestroysMounts();
    testUiClearResetsMounts();
    testAttachAndDetachMount();
    testManualRemoveMountRootDestroysMount();
    testInvalidMounts();
    return 0;
}
