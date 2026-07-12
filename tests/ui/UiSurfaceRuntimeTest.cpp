#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "UiSystem.h"

namespace
{
    struct EventRecord
    {
        UiEventType type = UiEventType::PointerMove;
        UiEventPhase phase = UiEventPhase::None;
        UiSurfaceId surface = UI_INVALID_SURFACE;
        UiHandle target = UI_INVALID_HANDLE;
        float pointerX = 0.0f;
        float pointerY = 0.0f;
    };

    struct ProbeState
    {
        std::vector<EventRecord> records;
        UiHandle button = UI_INVALID_HANDLE;
        int builds = 0;
        int destroys = 0;
    };

    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI surface runtime test failed: " << message << std::endl;
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

    void dispatchClick(UiSystem& ui, float x = 0.5f, float y = 0.5f)
    {
        ui.dispatchInput(pointerFrame(x, y), 1);
        ui.dispatchInput(pressFrame(x, y), 2);
        ui.dispatchInput(releaseFrame(x, y), 3);
    }

    class ProbeComposition : public UiComposition
    {
    public:
        explicit ProbeComposition(ProbeState& inState)
            : state(inState)
        {
        }

        void build(UiCompositionContext& context) override
        {
            ++state.builds;
            state.button = context.ui.createNode(context.surface, UiNodeType::Rect, context.root);
            context.ui.setLayout(context.surface, state.button, rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
            context.ui.setState(context.surface,
                                state.button,
                                UiStateFlags{.visible = true, .enabled = true, .interactable = true});
            context.ui.setPayload(context.surface,
                                  state.button,
                                  UiRectData{UiColor{1.0f, 1.0f, 1.0f, 1.0f}});
        }

        void onEvent(UiCompositionContext& /*context*/, UiEvent& event) override
        {
            state.records.push_back(EventRecord{
                event.type,
                event.phase,
                event.surface,
                event.target,
                event.pointerX,
                event.pointerY
            });
        }

        void destroy(UiCompositionContext& context) override
        {
            ++state.destroys;
            if (state.button != UI_INVALID_HANDLE)
                context.ui.removeNode(context.surface, state.button);
            state.button = UI_INVALID_HANDLE;
        }

    private:
        ProbeState& state;
    };

    UiSurfaceDesc surfaceDesc(const std::string& name, int order, UiRect rect = {0.0f, 0.0f, 1.0f, 1.0f})
    {
        UiSurfaceDesc desc;
        desc.name = name;
        desc.kind = UiSurfaceKind::Screen;
        desc.order = order;
        desc.rect = rect;
        return desc;
    }

    bool hasEvent(const ProbeState& state, UiEventType type)
    {
        for (const EventRecord& record : state.records)
        {
            if (record.type == type)
                return true;
        }
        return false;
    }

    void testDefaultSurfaceCompatibility()
    {
        UiSystem ui(nullptr);
        require(ui.getDefaultSurface() == UI_DEFAULT_SURFACE, "default surface id changed");
        require(ui.surfaceIds().size() == 1, "ui did not start with exactly one default surface");

        ProbeState state;
        ui.mountComposition(std::make_unique<ProbeComposition>(state));
        dispatchClick(ui);

        require(ui.dispatchResult().surface == UI_DEFAULT_SURFACE, "default dispatch used wrong surface");
        require(hasEvent(state, UiEventType::PointerClick), "default surface did not deliver click");
        require(ui.findNode(state.button) != nullptr, "default-surface findNode did not use default surface");
    }

    void testOverlappingSurfaceOrderingRoutesInputToTopSurface()
    {
        UiSystem ui(nullptr);
        const UiSurfaceId overlay = ui.createSurface(surfaceDesc("overlay", 100));

        ProbeState background;
        ProbeState foreground;
        ui.mountComposition(std::make_unique<ProbeComposition>(background));
        ui.mountComposition(overlay, std::make_unique<ProbeComposition>(foreground));

        dispatchClick(ui);

        require(ui.dispatchResult().surface == overlay, "top surface did not own overlapping input");
        require(!hasEvent(background, UiEventType::PointerClick), "lower surface received top-surface click");
        require(hasEvent(foreground, UiEventType::PointerClick), "top surface did not receive click");
    }

    void testHiddenAndDisabledSurfacesDoNotParticipateInInput()
    {
        UiSystem ui(nullptr);
        const UiSurfaceId overlay = ui.createSurface(surfaceDesc("overlay", 100));

        ProbeState background;
        ProbeState foreground;
        ui.mountComposition(std::make_unique<ProbeComposition>(background));
        ui.mountComposition(overlay, std::make_unique<ProbeComposition>(foreground));

        UiSurfaceDesc hidden = surfaceDesc("overlay", 100);
        hidden.visible = false;
        require(ui.setSurfaceDesc(overlay, hidden), "surface hide failed");
        dispatchClick(ui);
        require(ui.dispatchResult().surface == UI_DEFAULT_SURFACE, "hidden surface still received input");
        require(hasEvent(background, UiEventType::PointerClick), "default surface did not receive hidden-overlay click");
        require(!hasEvent(foreground, UiEventType::PointerClick), "hidden surface received click");

        background.records.clear();
        foreground.records.clear();
        hidden.visible = true;
        hidden.inputEnabled = false;
        require(ui.setSurfaceDesc(overlay, hidden), "surface input disable failed");
        dispatchClick(ui);
        require(ui.dispatchResult().surface == UI_DEFAULT_SURFACE, "input-disabled surface still received input");
        require(hasEvent(background, UiEventType::PointerClick), "default surface did not receive disabled-overlay click");
    }

    void testSurfaceCoordinateConversion()
    {
        UiSystem ui(nullptr);
        const UiSurfaceId inset = ui.createSurface(surfaceDesc("inset", 50, UiRect{0.25f, 0.25f, 0.5f, 0.5f}));

        ProbeState state;
        ui.mountComposition(inset, std::make_unique<ProbeComposition>(state));
        dispatchClick(ui, 0.5f, 0.5f);

        require(ui.dispatchResult().surface == inset, "inset surface did not receive input");
        bool sawLocalCenter = false;
        for (const EventRecord& record : state.records)
        {
            if (record.type == UiEventType::PointerClick &&
                record.phase == UiEventPhase::Target &&
                record.pointerX > 0.49f &&
                record.pointerX < 0.51f &&
                record.pointerY > 0.49f &&
                record.pointerY < 0.51f)
            {
                sawLocalCenter = true;
            }
        }
        require(sawLocalCenter, "surface did not convert screen coordinates to local coordinates");
    }

    void testFocusAndModalStateAreSurfaceLocal()
    {
        UiSystem ui(nullptr);
        const UiSurfaceId overlay = ui.createSurface(surfaceDesc("overlay", 100));

        ProbeState background;
        ProbeState foreground;
        ui.mountComposition(std::make_unique<ProbeComposition>(background));
        ui.mountComposition(overlay, std::make_unique<ProbeComposition>(foreground));

        UiSurface* defaultSurface = ui.findSurface(UI_DEFAULT_SURFACE);
        UiSurface* overlaySurface = ui.findSurface(overlay);
        require(defaultSurface != nullptr && overlaySurface != nullptr, "surface lookup failed");
        require(defaultSurface->focusManager().requestFocus(defaultSurface->document(), background.button),
                "default focus failed");
        require(overlaySurface->focusManager().requestFocus(overlaySurface->document(), foreground.button),
                "overlay focus failed");
        require(defaultSurface->focusManager().focusedNode() == background.button,
                "default focus was not isolated");
        require(overlaySurface->focusManager().focusedNode() == foreground.button,
                "overlay focus was not isolated");

        UiModalDesc modal;
        modal.owner = foreground.button;
        modal.initialFocus = foreground.button;
        modal.dismissOnCancel = false;
        require(ui.pushModal(overlay, modal) != UI_INVALID_MODAL, "overlay modal push failed");
        require(!defaultSurface->modalManager().hasModal(), "default surface inherited overlay modal");
        require(overlaySurface->modalManager().hasModal(), "overlay modal missing");

        UiInputFrame cancel = pointerFrame(0.5f, 0.5f);
        cancel.cancelPressed = true;
        ui.dispatchInput(cancel, 10);
        require(ui.dispatchResult().surface == overlay, "cancel did not route to overlay modal surface");
        require(hasEvent(foreground, UiEventType::NavigationCancel), "overlay modal owner did not receive cancel");
    }

    void testMountCompositionAndDestroySurfaceCleanup()
    {
        UiSystem ui(nullptr);
        const UiSurfaceId overlay = ui.createSurface(surfaceDesc("overlay", 100));

        ProbeState state;
        const UiCompositionId composition = ui.mountComposition(overlay, std::make_unique<ProbeComposition>(state));
        const UiMountId mount = ui.compositionMount(composition);

        require(composition != UI_INVALID_COMPOSITION, "surface composition creation failed");
        require(ui.compositionSurface(composition) == overlay, "composition did not remember surface");
        require(ui.compositionFromMount(overlay, mount) == composition, "surface mount did not resolve composition");
        require(ui.mountRoot(overlay, mount) != UI_INVALID_HANDLE, "surface mount root missing");

        UiSurface* surface = ui.findSurface(overlay);
        require(surface != nullptr, "overlay surface missing before destroy");
        require(surface->focusManager().capturePointer(surface->document(), UI_PRIMARY_POINTER, state.button),
                "surface capture failed");

        UiModalDesc modal;
        modal.owner = state.button;
        modal.initialFocus = state.button;
        require(ui.pushModal(overlay, modal) != UI_INVALID_MODAL, "surface modal push failed");

        require(ui.destroySurface(overlay), "surface destroy failed");
        require(state.destroys == 1, "destroying surface did not destroy composition");
        require(ui.findSurface(overlay) == nullptr, "destroyed surface remained registered");
        require(ui.findComposition(composition) == nullptr, "destroyed surface kept composition");
    }

    void testRenderExtractionCombinesOrderedSurfaces()
    {
        UiSystem ui(nullptr);
        const UiSurfaceId right = ui.createSurface(surfaceDesc("right", 50, UiRect{0.5f, 0.0f, 0.5f, 1.0f}));

        ProbeState leftState;
        ProbeState rightState;
        ui.mountComposition(std::make_unique<ProbeComposition>(leftState));
        ui.mountComposition(right, std::make_unique<ProbeComposition>(rightState));

        const UiCommandBuffer& commands = ui.extractCommandsRef(800, 600);
        require(commands.commands.size() >= 2, "combined extraction did not include both surfaces");

        bool sawRightSurfaceVertex = false;
        for (const UiVertex& vertex : commands.vertices)
        {
            if (vertex.pos.x >= 0.5f)
                sawRightSurfaceVertex = true;
        }
        require(sawRightSurfaceVertex, "surface rect was not applied during command extraction");
    }

    void testClearRestoresOnlyDefaultSurface()
    {
        UiSystem ui(nullptr);
        const UiSurfaceId overlay = ui.createSurface(surfaceDesc("overlay", 100));
        ProbeState state;
        ui.mountComposition(overlay, std::make_unique<ProbeComposition>(state));

        ui.clear();

        require(state.destroys == 1, "clear did not destroy surface composition");
        require(ui.findSurface(overlay) == nullptr, "clear kept custom surface");
        require(ui.surfaceIds().size() == 1, "clear did not restore exactly one surface");
        require(ui.findSurface(UI_DEFAULT_SURFACE) != nullptr, "clear did not restore default surface");
    }
}

int main()
{
    testDefaultSurfaceCompatibility();
    testOverlappingSurfaceOrderingRoutesInputToTopSurface();
    testHiddenAndDisabledSurfacesDoNotParticipateInInput();
    testSurfaceCoordinateConversion();
    testFocusAndModalStateAreSurfaceLocal();
    testMountCompositionAndDestroySurfaceCleanup();
    testRenderExtractionCombinesOrderedSurfaces();
    testClearRestoresOnlyDefaultSurface();
    return 0;
}
