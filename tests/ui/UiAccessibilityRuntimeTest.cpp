#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "UiSystem.h"
#include "UiWidget.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI accessibility runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    bool near(float lhs, float rhs, float epsilon = 0.001f)
    {
        return std::fabs(lhs - rhs) <= epsilon;
    }

    UiLayoutSpec fixedLayout(float width, float height)
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Absolute;
        layout.widthMode = UiSizeMode::Fixed;
        layout.heightMode = UiSizeMode::Fixed;
        layout.fixedWidth = width;
        layout.fixedHeight = height;
        return layout;
    }

    UiLayoutSpec box(float x, float y, float width, float height)
    {
        UiLayoutSpec layout = fixedLayout(width, height);
        layout.offsetMin = {x, y};
        return layout;
    }

    UiInputFrame pointerFrame(float x, float y)
    {
        UiInputFrame frame;
        frame.pointerX = x;
        frame.pointerY = y;
        return frame;
    }

    UiInputFrame pressFrame(float x, float y)
    {
        UiInputFrame frame = pointerFrame(x, y);
        frame.primaryDown = true;
        frame.primaryPressed = true;
        return frame;
    }

    UiInputFrame releaseFrame(float x, float y)
    {
        UiInputFrame frame = pointerFrame(x, y);
        frame.primaryReleased = true;
        return frame;
    }

    void extract(UiSystem& ui)
    {
        ui.extractCommandsRef(800, 600);
    }

    const UiSemanticNode* findSemantic(const UiSemanticTree& tree,
                                       UiSemanticRole role,
                                       const std::string& name)
    {
        for (const UiSemanticNode& node : tree.nodes)
        {
            if (node.role == role && node.name == name)
                return &node;
        }
        return nullptr;
    }

    bool hasAnnouncement(const std::vector<UiAccessibilityAnnouncement>& announcements,
                         UiAccessibilityAnnouncementKind kind,
                         const std::string& text)
    {
        return std::any_of(announcements.begin(),
                           announcements.end(),
                           [&](const UiAccessibilityAnnouncement& announcement)
                           {
                               return announcement.kind == kind && announcement.text == text;
                           });
    }

    UiHandle createText(UiSystem& ui,
                        UiSurfaceId surface,
                        UiHandle parent,
                        const std::string& text)
    {
        const UiHandle handle = ui.createNode(surface, UiNodeType::Text, parent);
        ui.setLayout(surface, handle, box(0.0f, 0.0f, 0.4f, 0.1f));
        UiTextData payload;
        payload.text = text;
        ui.setPayload(surface, handle, payload);
        return handle;
    }

    struct ButtonProbeState
    {
        UiHandle button = UI_INVALID_HANDLE;
        int pressed = 0;
    };

    class ButtonProbeComposition : public UiComposition
    {
    public:
        explicit ButtonProbeComposition(ButtonProbeState& inState)
            : state(inState)
        {
        }

        void build(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);

            gts::ui::UiButtonDesc desc;
            desc.layout = box(0.20f, 0.20f, 0.30f, 0.15f);
            desc.text = "Apply";
            desc.onPressed = [&state = state]()
            {
                ++state.pressed;
            };
            button.build(widgetContext, context.root, desc);
            state.button = button.root();
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            button.onEvent(widgetContext, event);
        }

        void destroy(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            button.destroy(widgetContext);
            state.button = UI_INVALID_HANDLE;
        }

    private:
        ButtonProbeState& state;
        gts::ui::UiButtonWidget button;
    };

    void testWidgetSemanticTreeRolesAndRelationships()
    {
        UiSystem ui(nullptr);
        gts::ui::UiWidgetContext widgetContext(ui);

        gts::ui::UiLabelWidget label;
        gts::ui::UiLabelDesc labelDesc;
        labelDesc.layout = box(0.0f, 0.0f, 0.40f, 0.08f);
        labelDesc.text = "Status ready";
        label.build(widgetContext, ui.getRoot(), labelDesc);

        gts::ui::UiButtonWidget button;
        gts::ui::UiButtonDesc buttonDesc;
        buttonDesc.layout = box(0.0f, 0.10f, 0.40f, 0.10f);
        buttonDesc.text = "Start";
        button.build(widgetContext, ui.getRoot(), buttonDesc);

        gts::ui::UiProgressBarWidget progress;
        gts::ui::UiProgressBarDesc progressDesc;
        progressDesc.layout = box(0.0f, 0.25f, 0.50f, 0.08f);
        progressDesc.value = 0.50f;
        progressDesc.accessibilityName = "Health";
        progress.build(widgetContext, ui.getRoot(), progressDesc);

        ui.updateAccessibility();
        const UiSemanticTree tree = ui.accessibilityTree();
        require(findSemantic(tree, UiSemanticRole::Label, "Status ready") != nullptr,
                "label semantic node was not created");

        const UiSemanticNode* buttonNode = findSemantic(tree, UiSemanticRole::Button, "Start");
        require(buttonNode != nullptr, "button semantic name did not resolve from label");
        require(!buttonNode->relationships.labelledBy.empty(),
                "button did not preserve label relationship");

        const UiSemanticNode* progressNode = findSemantic(tree, UiSemanticRole::ProgressBar, "Health");
        require(progressNode != nullptr, "progress bar semantic node was not created");
        require(progressNode->hasRange && near(progressNode->rangeValue, 0.50f),
                "progress bar semantic range was not retained");

        progress.destroy(widgetContext);
        button.destroy(widgetContext);
        label.destroy(widgetContext);
    }

    void testFocusAndButtonAnnouncements()
    {
        UiSystem ui(nullptr);
        ButtonProbeState state;
        ui.mountComposition(std::make_unique<ButtonProbeComposition>(state));
        extract(ui);
        ui.updateAccessibility();
        ui.drainAccessibilityAnnouncements();

        const UiNode* buttonNode = ui.findNode(state.button);
        require(buttonNode != nullptr, "button node missing");
        const UiRect bounds = buttonNode->computedLayout.bounds;
        const float x = bounds.x + bounds.width * 0.5f;
        const float y = bounds.y + bounds.height * 0.5f;

        ui.dispatchInput(pointerFrame(x, y), 1);
        ui.dispatchInput(pressFrame(x, y), 2);
        ui.dispatchInput(releaseFrame(x, y), 3);

        require(state.pressed == 1, "button semantic action did not run");
        buttonNode = ui.findNode(state.button);
        require(buttonNode != nullptr && buttonNode->state.focused,
                "button focus did not update retained state");

        const std::vector<UiAccessibilityAnnouncement> announcements =
            ui.drainAccessibilityAnnouncements();
        require(hasAnnouncement(announcements, UiAccessibilityAnnouncementKind::FocusChanged, "Apply"),
                "focus announcement was not emitted");
        require(hasAnnouncement(announcements, UiAccessibilityAnnouncementKind::Action, "Apply"),
                "button action announcement was not emitted");
    }

    void testLiveRegionBinding()
    {
        UiSystem ui(nullptr);
        gts::ui::UiWidgetContext widgetContext(ui);

        gts::ui::UiLabelWidget prompt;
        gts::ui::UiLabelDesc desc;
        desc.layout = box(0.0f, 0.0f, 0.50f, 0.10f);
        desc.semanticRole = UiSemanticRole::Status;
        desc.accessibilityName = "Interaction prompt";
        desc.liveRegion = UiAccessibilityLiveRegion::Polite;
        prompt.build(widgetContext, ui.getRoot(), desc);

        UiObservable<std::string> promptText("Open door");
        prompt.bindText(widgetContext, bindObservable(promptText));
        ui.updateAccessibility();
        ui.drainAccessibilityAnnouncements();

        promptText.set("Talk");
        ui.updateBindings();

        const std::vector<UiAccessibilityAnnouncement> announcements =
            ui.drainAccessibilityAnnouncements();
        require(hasAnnouncement(announcements, UiAccessibilityAnnouncementKind::LiveRegion, "Talk"),
                "live region did not announce bound text change");

        prompt.destroy(widgetContext);
    }

    void testProgressBindingUpdatesSemanticRange()
    {
        UiSystem ui(nullptr);
        gts::ui::UiWidgetContext widgetContext(ui);

        gts::ui::UiProgressBarWidget progress;
        gts::ui::UiProgressBarDesc desc;
        desc.layout = box(0.0f, 0.0f, 0.50f, 0.08f);
        desc.value = 0.10f;
        desc.accessibilityName = "Health";
        desc.liveRegion = UiAccessibilityLiveRegion::Polite;
        progress.build(widgetContext, ui.getRoot(), desc);

        UiObservable<double> health(0.40);
        progress.bindValue(widgetContext, bindObservable(health));
        ui.updateAccessibility();
        UiSemanticTree tree = ui.accessibilityTree();
        const UiSemanticNode* node = findSemantic(tree, UiSemanticRole::ProgressBar, "Health");
        require(node != nullptr && near(node->rangeValue, 0.40f),
                "initial progress binding did not update semantic range");
        ui.drainAccessibilityAnnouncements();

        health.set(0.75);
        ui.updateBindings();
        tree = ui.accessibilityTree();
        node = findSemantic(tree, UiSemanticRole::ProgressBar, "Health");
        require(node != nullptr && near(node->rangeValue, 0.75f),
                "progress binding change did not update semantic range");

        const std::vector<UiAccessibilityAnnouncement> announcements =
            ui.drainAccessibilityAnnouncements();
        require(hasAnnouncement(announcements, UiAccessibilityAnnouncementKind::LiveRegion, "0.75"),
                "progress live region did not announce value change");

        progress.destroy(widgetContext);
    }

    void testMountCleanupAndSurfaceIsolation()
    {
        UiSystem ui(nullptr);

        const UiHandle defaultText = createText(ui,
                                                ui.getDefaultSurface(),
                                                ui.getRoot(),
                                                "Default");
        UiSemanticDesc defaultSemantic;
        defaultSemantic.role = UiSemanticRole::Label;
        defaultSemantic.name = "Default";
        require(ui.setSemantics(defaultText, defaultSemantic),
                "default surface semantics were not registered");

        UiSurfaceDesc surfaceDesc;
        surfaceDesc.name = "accessibility";
        surfaceDesc.order = 10;
        surfaceDesc.rect = {0.0f, 0.0f, 1.0f, 1.0f};
        const UiSurfaceId surface = ui.createSurface(surfaceDesc);
        const UiHandle surfaceText = createText(ui, surface, ui.getRoot(surface), "Surface");
        UiSemanticDesc surfaceSemantic;
        surfaceSemantic.role = UiSemanticRole::Label;
        surfaceSemantic.name = "Surface";
        require(ui.setSemantics(surface, surfaceText, surfaceSemantic),
                "secondary surface semantics were not registered");

        require(ui.accessibilityTree().nodes.size() == 1,
                "default surface accessibility tree leaked secondary semantics");
        require(ui.accessibilityTree(surface).nodes.size() == 1,
                "secondary surface accessibility tree missing semantic node");
        require(ui.destroySurface(surface), "destroying secondary surface failed");
        require(ui.accessibilityTree().nodes.size() == 1,
                "destroying secondary surface removed default semantics");

        const UiMountId mount = ui.createMount();
        const UiHandle mountedText = createText(ui, ui.getDefaultSurface(), ui.mountRoot(mount), "Mounted");
        UiSemanticDesc mountSemantic;
        mountSemantic.role = UiSemanticRole::Label;
        mountSemantic.name = "Mounted";
        require(ui.setSemantics(mountedText, mountSemantic),
                "mounted semantics were not registered");
        require(ui.accessibilityManager().semanticCount() == 2,
                "mounted semantic count incorrect before cleanup");
        require(ui.destroyMount(mount), "mount destruction failed");
        require(ui.accessibilityManager().semanticCount() == 1,
                "mounted semantics survived mount destruction");
    }
}

int main()
{
    testWidgetSemanticTreeRolesAndRelationships();
    testFocusAndButtonAnnouncements();
    testLiveRegionBinding();
    testProgressBindingUpdatesSemanticRange();
    testMountCleanupAndSurfaceIsolation();
    return 0;
}
