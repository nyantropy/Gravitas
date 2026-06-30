#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <variant>

#include "UiSystem.h"

namespace
{
    struct ProbeState
    {
        int builds = 0;
        int updates = 0;
        int destroys = 0;
        UiHandle child = UI_INVALID_HANDLE;
        UiHandle text = UI_INVALID_HANDLE;
        bool visible = true;
        std::string label;
    };

    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI composition runtime test failed: " << message << std::endl;
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

    class ProbeComposition : public UiComposition
    {
    public:
        explicit ProbeComposition(ProbeState& inProbe)
            : probe(inProbe)
        {
        }

        void build(UiCompositionContext& context) override
        {
            ++probe.builds;
            probe.child = context.ui.createNode(UiNodeType::Rect, context.root);
            context.ui.setLayout(probe.child, rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
            context.ui.setState(probe.child, UiStateFlags{.visible = true, .enabled = true, .interactable = true});
            context.ui.setPayload(probe.child, UiRectData{UiColor{1.0f, 1.0f, 1.0f, 1.0f}});

            probe.text = context.ui.createNode(UiNodeType::Text, context.root);
            context.ui.setLayout(probe.text, rectLayout(0.0f, 0.0f, 1.0f, 0.2f));
            context.ui.setState(probe.text, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
        }

        void update(UiCompositionContext& context) override
        {
            ++probe.updates;
            context.ui.setState(probe.child, UiStateFlags{.visible = probe.visible,
                                                          .enabled = true,
                                                          .interactable = true});
            context.ui.setPayload(probe.text, UiTextData{probe.label, {}, UiColor{1.0f, 1.0f, 1.0f, 1.0f}, 0.02f});
        }

        void destroy(UiCompositionContext& context) override
        {
            ++probe.destroys;
            if (probe.child != UI_INVALID_HANDLE)
                context.ui.removeNode(probe.child);
            if (probe.text != UI_INVALID_HANDLE)
                context.ui.removeNode(probe.text);
            probe.child = UI_INVALID_HANDLE;
            probe.text = UI_INVALID_HANDLE;
        }

    private:
        ProbeState& probe;
    };

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

    void testCompositionCreationAndMountIntegration()
    {
        UiSystem ui(nullptr);
        ProbeState probe;
        const UiCompositionId composition = ui.mountComposition(std::make_unique<ProbeComposition>(probe));
        const UiMountId mount = ui.compositionMount(composition);

        require(composition != UI_INVALID_COMPOSITION, "composition creation failed");
        require(mount != UI_INVALID_MOUNT, "composition did not create a mount");
        require(probe.builds == 1, "composition build was not called");
        require(ui.findComposition(composition) != nullptr, "composition was not registered");
        require(ui.compositionFromMount(mount) == composition, "mount did not resolve to composition");
        require(ui.findNode(probe.child) != nullptr, "composition child was not created");
        require(ui.mountFromNode(probe.child) == mount, "composition child did not resolve to mount");
    }

    void testAttachCompositionToExistingMount()
    {
        UiSystem ui(nullptr);
        ProbeState probe;
        const UiMountId mount = ui.createMount();
        const UiCompositionId composition =
            ui.attachComposition(mount, std::make_unique<ProbeComposition>(probe));

        require(composition != UI_INVALID_COMPOSITION, "attach composition failed");
        require(ui.compositionMount(composition) == mount, "attached composition did not use existing mount");
        require(ui.mountFromNode(probe.child) == mount, "attached composition child did not resolve to existing mount");
        require(ui.attachComposition(mount, std::make_unique<ProbeComposition>(probe)) == UI_INVALID_COMPOSITION,
                "second composition attached to same mount");
    }

    void testCompositionUpdateAndParameters()
    {
        UiSystem ui(nullptr);
        ProbeState probe;
        const UiCompositionId composition = ui.mountComposition(std::make_unique<ProbeComposition>(probe));

        probe.visible = false;
        probe.label = "updated";
        require(ui.updateComposition(composition), "composition update failed");
        require(probe.updates == 1, "composition update hook was not called");

        const UiNode* child = ui.findNode(probe.child);
        require(child != nullptr && !child->state.visible, "composition update did not mutate state");
        const UiNode* text = ui.findNode(probe.text);
        require(text != nullptr, "composition text missing after update");
        require(std::get<UiTextData>(text->payload).text == "updated", "composition update did not apply label");
    }

    void testCompositionRebuild()
    {
        UiSystem ui(nullptr);
        ProbeState probe;
        const UiCompositionId composition = ui.mountComposition(std::make_unique<ProbeComposition>(probe));
        const UiHandle oldChild = probe.child;

        require(ui.rebuildComposition(composition), "composition rebuild failed");
        require(probe.builds == 2, "rebuild did not call build again");
        require(probe.destroys == 1, "rebuild did not call destroy");
        require(ui.findNode(oldChild) == nullptr, "rebuild left old child alive");
        require(probe.child != UI_INVALID_HANDLE && probe.child != oldChild, "rebuild did not create a new child");
        require(ui.findNode(probe.child) != nullptr, "rebuild child is missing");
    }

    void testDestroyComposition()
    {
        UiSystem ui(nullptr);
        ProbeState probe;
        const UiCompositionId composition = ui.mountComposition(std::make_unique<ProbeComposition>(probe));
        const UiMountId mount = ui.compositionMount(composition);
        const UiHandle child = probe.child;

        require(ui.destroyComposition(composition), "composition destroy failed");
        require(probe.destroys == 1, "composition destroy hook was not called");
        require(ui.findComposition(composition) == nullptr, "destroyed composition remained registered");
        require(ui.findMount(mount) == nullptr, "composition destroy left mount registered");
        require(ui.findNode(child) == nullptr, "composition destroy left child node");
    }

    void testNestedCompositionCleanup()
    {
        UiSystem ui(nullptr);
        ProbeState parentProbe;
        ProbeState childProbe;
        const UiCompositionId parent = ui.mountComposition(std::make_unique<ProbeComposition>(parentProbe));
        const UiMountId parentMount = ui.compositionMount(parent);
        const UiMountDesc childDesc = childMount(parentMount, "child-composition");
        const UiCompositionId child = ui.mountComposition(std::make_unique<ProbeComposition>(childProbe), childDesc);
        const UiMountId childMountId = ui.compositionMount(child);

        require(ui.destroyMount(parentMount), "parent mount destroy failed");
        require(parentProbe.destroys == 1, "parent composition destroy hook was not called");
        require(childProbe.destroys == 1, "child composition destroy hook was not called");
        require(ui.findComposition(parent) == nullptr, "parent composition remained registered");
        require(ui.findComposition(child) == nullptr, "child composition remained registered");
        require(ui.findMount(parentMount) == nullptr, "parent mount remained registered");
        require(ui.findMount(childMountId) == nullptr, "child mount remained registered");
    }

    void testFocusAndModalCleanup()
    {
        UiSystem ui(nullptr);
        ProbeState probe;
        const UiLayerId layer = ui.createLayer("modal", 100);
        const UiCompositionId composition =
            ui.mountComposition(std::make_unique<ProbeComposition>(probe), mountInLayer(layer, "modal-owner"));
        const UiMountId mount = ui.compositionMount(composition);

        UiFocusManager& focus = ui.focusManager();
        require(focus.capturePointer(ui.getDocument(), UI_PRIMARY_POINTER, probe.child), "capture setup failed");
        require(focus.requestFocus(ui.getDocument(), probe.child), "focus setup failed");

        UiModalDesc modalDesc;
        modalDesc.layer = layer;
        modalDesc.owner = probe.child;
        modalDesc.initialFocus = probe.child;
        const UiModalId modal = ui.pushModal(modalDesc);
        require(modal != UI_INVALID_MODAL, "modal setup failed");

        require(ui.destroyComposition(composition), "composition destroy with focus/modal failed");
        require(focus.capturedNode() == UI_INVALID_HANDLE, "capture survived composition destroy");
        require(focus.focusedNode() == UI_INVALID_HANDLE, "focus survived composition destroy");
        require(!ui.modalManager().hasModal(), "modal survived composition destroy");
        require(ui.findMount(mount) == nullptr, "modal owner mount survived composition destroy");
    }

    void testLayerRemovalDestroysComposition()
    {
        UiSystem ui(nullptr);
        ProbeState probe;
        const UiLayerId layer = ui.createLayer("overlay", 50);
        const UiCompositionId composition =
            ui.mountComposition(std::make_unique<ProbeComposition>(probe), mountInLayer(layer, "overlay-composition"));

        require(ui.removeLayer(layer), "layer removal failed");
        require(probe.destroys == 1, "layer removal did not destroy composition");
        require(ui.findComposition(composition) == nullptr, "layer removal left composition registered");
    }

    void testClearDestroysCompositionRecords()
    {
        UiSystem ui(nullptr);
        ProbeState probe;
        const UiCompositionId composition = ui.mountComposition(std::make_unique<ProbeComposition>(probe));
        const UiMountId mount = ui.compositionMount(composition);

        ui.clear();
        require(probe.destroys == 1, "clear did not call composition destroy");
        require(ui.findComposition(composition) == nullptr, "clear kept composition registered");
        require(ui.findMount(mount) == nullptr, "clear kept composition mount registered");
        require(ui.findMount(ui.rootMount()) != nullptr, "clear did not restore root mount");
    }
}

int main()
{
    testCompositionCreationAndMountIntegration();
    testAttachCompositionToExistingMount();
    testCompositionUpdateAndParameters();
    testCompositionRebuild();
    testDestroyComposition();
    testNestedCompositionCleanup();
    testFocusAndModalCleanup();
    testLayerRemovalDestroysComposition();
    testClearDestroysCompositionRecords();
    return 0;
}
