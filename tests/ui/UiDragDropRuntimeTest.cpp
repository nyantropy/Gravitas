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
            std::cerr << "UI drag/drop runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    UiLayoutSpec fillLayout()
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Anchored;
        layout.widthMode = UiSizeMode::FromAnchors;
        layout.heightMode = UiSizeMode::FromAnchors;
        layout.anchorMin = {0.0f, 0.0f};
        layout.anchorMax = {1.0f, 1.0f};
        return layout;
    }

    UiLayoutSpec box(float x, float y, float width, float height)
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

    UiInputFrame pointerFrame(float x, float y, bool down, bool pressed, bool released)
    {
        UiInputFrame frame;
        frame.pointerX = x;
        frame.pointerY = y;
        frame.primaryDown = down;
        frame.primaryPressed = pressed;
        frame.primaryReleased = released;
        return frame;
    }

    UiInputFrame cancelFrame(float x, float y)
    {
        UiInputFrame frame = pointerFrame(x, y, true, false, false);
        frame.cancelPressed = true;
        return frame;
    }

    const UiEvent* findLastEvent(const UiSystem& ui, UiEventType type)
    {
        const auto& events = ui.events();
        for (auto it = events.rbegin(); it != events.rend(); ++it)
        {
            if (it->type == type)
                return &*it;
        }
        return nullptr;
    }

    struct EventRecord
    {
        UiEventType type = UiEventType::PointerMove;
        UiHandle target = UI_INVALID_HANDLE;
        UiHandle dragSource = UI_INVALID_HANDLE;
        UiHandle dragTarget = UI_INVALID_HANDLE;
        UiDragPayload payload;
        bool accepted = false;
    };

    struct DragProbeState
    {
        UiHandle source = UI_INVALID_HANDLE;
        UiHandle target = UI_INVALID_HANDLE;
        UiHandle rejectTarget = UI_INVALID_HANDLE;
        std::vector<EventRecord> targetEvents;
    };

    UiDragSourceDesc sourceDesc(const std::string& type = "probe.payload")
    {
        UiDragSourceDesc desc;
        desc.payload.type = type;
        desc.payload.id = 42;
        desc.payload.label = "payload";
        desc.startThreshold = 0.0f;
        return desc;
    }

    UiDropTargetDesc targetDesc(const std::string& type = "probe.payload")
    {
        UiDropTargetDesc desc;
        desc.acceptedPayloadTypes.push_back(type);
        return desc;
    }

    class DragProbeComposition : public UiComposition
    {
    public:
        explicit DragProbeComposition(DragProbeState& inState)
            : state(inState)
        {
        }

        void build(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            context.ui.setLayout(context.root, fillLayout());

            gts::ui::UiPanelDesc sourcePanel;
            sourcePanel.layout = box(0.10f, 0.10f, 0.18f, 0.12f);
            sourcePanel.interactable = true;
            sourcePanel.dragSource = sourceDesc();
            source.build(widgetContext, context.root, sourcePanel);
            state.source = source.root();

            gts::ui::UiPanelDesc targetPanel;
            targetPanel.layout = box(0.55f, 0.10f, 0.18f, 0.12f);
            targetPanel.interactable = true;
            targetPanel.dropTarget = targetDesc();
            target.build(widgetContext, context.root, targetPanel);
            state.target = target.root();

            gts::ui::UiPanelDesc rejectPanel;
            rejectPanel.layout = box(0.55f, 0.35f, 0.18f, 0.12f);
            rejectPanel.interactable = true;
            rejectPanel.dropTarget = targetDesc("other.payload");
            rejectTarget.build(widgetContext, context.root, rejectPanel);
            state.rejectTarget = rejectTarget.root();
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            if (event.phase != UiEventPhase::Target)
                return;

            switch (event.type)
            {
                case UiEventType::DragStart:
                case UiEventType::DragMove:
                case UiEventType::DragEnter:
                case UiEventType::DragLeave:
                case UiEventType::DragOver:
                case UiEventType::DragDrop:
                case UiEventType::DragAccept:
                case UiEventType::DragReject:
                case UiEventType::DragCancel:
                case UiEventType::DragEnd:
                    state.targetEvents.push_back(EventRecord{
                        event.type,
                        event.target,
                        event.dragSource,
                        event.dragTarget,
                        event.dragPayload,
                        event.dragAccepted
                    });
                    break;
                default:
                    break;
            }

            gts::ui::UiWidgetContext widgetContext(context);
            source.onEvent(widgetContext, event);
            target.onEvent(widgetContext, event);
            rejectTarget.onEvent(widgetContext, event);
        }

        void destroy(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            rejectTarget.destroy(widgetContext);
            target.destroy(widgetContext);
            source.destroy(widgetContext);
        }

    private:
        DragProbeState& state;
        gts::ui::UiPanelWidget source;
        gts::ui::UiPanelWidget target;
        gts::ui::UiPanelWidget rejectTarget;
    };

    bool hasRecord(const DragProbeState& state, UiEventType type, UiHandle target = UI_INVALID_HANDLE)
    {
        for (const EventRecord& record : state.targetEvents)
        {
            if (record.type == type && (target == UI_INVALID_HANDLE || record.target == target))
                return true;
        }
        return false;
    }

    void extract(UiSystem& ui)
    {
        ui.extractCommandsRef(800, 600);
    }

    void testAcceptedDrop()
    {
        UiSystem ui(nullptr);
        DragProbeState state;
        ui.mountComposition(std::make_unique<DragProbeComposition>(state));
        extract(ui);

        const UiDispatchResult& start = ui.dispatchInput(pointerFrame(0.15f, 0.15f, true, true, false), 1);
        require(start.dragStarted, "drag did not start from widget source");
        require(start.dragSource == state.source, "drag source was not reported");
        require(start.dragPayload.type == "probe.payload", "drag payload type was not preserved");
        require(ui.focusManager().capturedNode() == state.source, "drag did not capture pointer");

        const UiDispatchResult& move = ui.dispatchInput(pointerFrame(0.60f, 0.15f, true, false, false), 2);
        require(move.dragging, "drag was not active during pointer move");
        require(move.dragEnteredTarget, "drag did not enter compatible target");
        require(move.dragTarget == state.target, "compatible drop target was not selected");

        const UiDispatchResult& drop = ui.dispatchInput(pointerFrame(0.60f, 0.15f, false, false, true), 3);
        require(drop.dragDropped, "accepted target did not receive drop");
        require(drop.dragAccepted, "drop was not marked accepted");
        require(drop.dragEnded, "accepted drop did not end drag");
        require(ui.focusManager().capturedNode() == UI_INVALID_HANDLE, "pointer capture survived accepted drop");

        require(hasRecord(state, UiEventType::DragStart, state.source), "DragStart was not delivered to source");
        require(hasRecord(state, UiEventType::DragEnter, state.target), "DragEnter was not delivered to target");
        require(hasRecord(state, UiEventType::DragDrop, state.target), "DragDrop was not delivered to target");
        require(hasRecord(state, UiEventType::DragAccept, state.target), "DragAccept was not delivered to target");
        require(hasRecord(state, UiEventType::DragEnd, state.source), "DragEnd was not delivered to source");

        const UiEvent* dropEvent = findLastEvent(ui, UiEventType::DragAccept);
        require(dropEvent != nullptr && dropEvent->dragPayload.id == 42, "drop event did not carry payload id");
    }

    void testRejectedDropAndClickSuppression()
    {
        UiSystem ui(nullptr);
        DragProbeState state;
        ui.mountComposition(std::make_unique<DragProbeComposition>(state));
        extract(ui);

        ui.dispatchInput(pointerFrame(0.15f, 0.15f, true, true, false), 1);
        ui.dispatchInput(pointerFrame(0.60f, 0.40f, true, false, false), 2);
        const UiDispatchResult& rejected = ui.dispatchInput(pointerFrame(0.60f, 0.40f, false, false, true), 3);

        require(rejected.dragRejected, "incompatible target did not reject drop");
        require(!rejected.dragDropped, "incompatible target reported accepted drop");
        require(rejected.clicked == UI_INVALID_HANDLE, "drag release produced a click");
        require(hasRecord(state, UiEventType::DragReject, state.source), "DragReject was not delivered to source");
        require(hasRecord(state, UiEventType::DragEnd, state.source), "DragEnd was not delivered after rejection");
    }

    void testCancelledDrag()
    {
        UiSystem ui(nullptr);
        DragProbeState state;
        ui.mountComposition(std::make_unique<DragProbeComposition>(state));
        extract(ui);

        ui.dispatchInput(pointerFrame(0.15f, 0.15f, true, true, false), 1);
        const UiDispatchResult& cancelled = ui.dispatchInput(cancelFrame(0.20f, 0.18f), 2);

        require(cancelled.dragCancelled, "cancel did not cancel active drag");
        require(cancelled.dragEnded, "cancelled drag did not end");
        require(ui.focusManager().capturedNode() == UI_INVALID_HANDLE, "pointer capture survived drag cancel");
        require(hasRecord(state, UiEventType::DragCancel, state.source), "DragCancel was not delivered");
        require(hasRecord(state, UiEventType::DragEnd, state.source), "DragEnd was not delivered after cancel");
    }

    void testDestroyedTargetRejectsDrop()
    {
        UiSystem ui(nullptr);
        DragProbeState state;
        ui.mountComposition(std::make_unique<DragProbeComposition>(state));
        extract(ui);

        ui.dispatchInput(pointerFrame(0.15f, 0.15f, true, true, false), 1);
        ui.dispatchInput(pointerFrame(0.60f, 0.15f, true, false, false), 2);
        require(ui.removeNode(state.target), "target removal failed");

        const UiDispatchResult& rejected = ui.dispatchInput(pointerFrame(0.60f, 0.15f, false, false, true), 3);
        require(rejected.dragRejected, "destroyed target did not reject drop");
        require(rejected.dragTarget == UI_INVALID_HANDLE, "destroyed target remained active");
    }

    void testModalBlocksLowerLayerDragSource()
    {
        UiSystem ui(nullptr);
        DragProbeState state;
        ui.mountComposition(std::make_unique<DragProbeComposition>(state));

        const UiLayerId modalLayer = ui.createLayer("modal", 10);
        const UiHandle modalRoot = ui.createNode(UiNodeType::Rect, ui.getLayerRoot(modalLayer));
        ui.setLayout(modalRoot, box(0.35f, 0.35f, 0.20f, 0.20f));
        ui.setState(modalRoot, UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        extract(ui);

        UiModalDesc modal;
        modal.layer = modalLayer;
        modal.owner = modalRoot;
        modal.consumePointer = true;
        modal.layerBlocking = UiModalLayerBlocking::AllOtherLayers;
        require(ui.pushModal(modal) != UI_INVALID_MODAL, "modal push failed");

        const UiDispatchResult& result = ui.dispatchInput(pointerFrame(0.15f, 0.15f, true, true, false), 1);
        require(!result.dragStarted, "lower layer source started drag through modal");
        require(result.pointerBlocked, "modal pointer block was not reported");
    }

    void testSurfaceIsolation()
    {
        UiSystem ui(nullptr);
        DragProbeState defaultState;
        ui.mountComposition(std::make_unique<DragProbeComposition>(defaultState));

        UiSurfaceDesc surfaceDesc;
        surfaceDesc.name = "secondary";
        surfaceDesc.order = 10;
        surfaceDesc.rect = {0.50f, 0.0f, 0.50f, 1.0f};
        const UiSurfaceId secondary = ui.createSurface(surfaceDesc);

        DragProbeState secondaryState;
        ui.mountComposition(secondary, std::make_unique<DragProbeComposition>(secondaryState));
        ui.extractSurfaceCommandsRef(secondary, 800, 600);
        extract(ui);

        ui.dispatchInput(pointerFrame(0.575f, 0.15f, true, true, false), 1);
        const UiDispatchResult& move = ui.dispatchInput(pointerFrame(0.10f, 0.15f, true, false, false), 2);
        require(move.surface == secondary, "drag capture did not keep secondary surface selected");
        require(move.dragging, "secondary surface drag did not continue outside rect");
        require(ui.focusManager().capturedNode() == UI_INVALID_HANDLE, "default surface captured secondary drag");

        const UiDispatchResult& release = ui.dispatchInput(pointerFrame(0.10f, 0.15f, false, false, true), 3);
        require(release.surface == secondary, "release did not route to captured secondary surface");
        require(release.dragRejected, "cross-surface drop was not rejected by surface-local drag runtime");
    }
}

int main()
{
    testAcceptedDrop();
    testRejectedDropAndClickSuppression();
    testCancelledDrag();
    testDestroyedTargetRejectsDrop();
    testModalBlocksLowerLayerDragSource();
    testSurfaceIsolation();
    return 0;
}
