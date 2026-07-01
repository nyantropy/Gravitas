#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "UiSystem.h"
#include "UiWidget.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI navigation runtime test failed: " << message << std::endl;
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

    UiInputFrame navFrame(UiNavigationDirection direction)
    {
        UiInputFrame frame;
        switch (direction)
        {
            case UiNavigationDirection::Up:
                frame.navigationUpPressed = true;
                break;
            case UiNavigationDirection::Down:
                frame.navigationDownPressed = true;
                break;
            case UiNavigationDirection::Left:
                frame.navigationLeftPressed = true;
                break;
            case UiNavigationDirection::Right:
                frame.navigationRightPressed = true;
                break;
            case UiNavigationDirection::Next:
                frame.navigationNextPressed = true;
                break;
            case UiNavigationDirection::Previous:
                frame.navigationPreviousPressed = true;
                break;
            case UiNavigationDirection::None:
                break;
        }
        return frame;
    }

    UiInputFrame submitFrame()
    {
        UiInputFrame frame;
        frame.navigationSubmitPressed = true;
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

    struct NavProbeState
    {
        static constexpr size_t Count = 4;

        std::array<UiHandle, Count> handles = {
            UI_INVALID_HANDLE,
            UI_INVALID_HANDLE,
            UI_INVALID_HANDLE,
            UI_INVALID_HANDLE
        };
        std::array<int, Count> pressed = {0, 0, 0, 0};
        std::array<bool, Count> enabled = {true, true, true, true};
        std::array<bool, Count> visible = {true, true, true, true};
        std::array<int, Count> tabIndex = {0, 1, 2, 3};
        std::array<UiLayoutSpec, Count> layouts = {
            box(0.10f, 0.10f, 0.20f, 0.10f),
            box(0.10f, 0.25f, 0.20f, 0.10f),
            box(0.40f, 0.10f, 0.20f, 0.10f),
            box(0.40f, 0.25f, 0.20f, 0.10f)
        };
        bool wrap = false;
        std::string group = "probe";
    };

    class NavProbeComposition : public UiComposition
    {
    public:
        explicit NavProbeComposition(NavProbeState& inState)
            : state(inState)
        {
        }

        void build(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            context.ui.setLayout(context.root, fillLayout());

            for (size_t i = 0; i < NavProbeState::Count; ++i)
            {
                gts::ui::UiButtonDesc desc;
                desc.layout = state.layouts[i];
                desc.text = "Button";
                desc.enabled = state.enabled[i];
                desc.visible = state.visible[i];
                desc.navigationGroup = state.group;
                desc.tabIndex = state.tabIndex[i];
                desc.wrapNavigation = state.wrap;
                desc.onPressed = [&state = state, i]()
                {
                    ++state.pressed[i];
                };

                buttons[i].build(widgetContext, context.root, desc);
                state.handles[i] = buttons[i].root();
            }
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            for (auto& button : buttons)
                button.onEvent(widgetContext, event);
        }

        void destroy(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            for (auto& button : buttons)
                button.destroy(widgetContext);
        }

    private:
        NavProbeState& state;
        std::array<gts::ui::UiButtonWidget, NavProbeState::Count> buttons;
    };

    void extract(UiSystem& ui)
    {
        ui.extractCommandsRef(800, 600);
    }

    void testTabOrderAndSubmitActivation()
    {
        UiSystem ui(nullptr);
        NavProbeState state;
        state.tabIndex = {20, 10, 30, 40};
        ui.mountComposition(std::make_unique<NavProbeComposition>(state));
        extract(ui);

        const UiDispatchResult& first = ui.dispatchInput(navFrame(UiNavigationDirection::Next), 1);
        require(first.navigationMoved, "next did not focus first tab-ordered node");
        require(first.navigationTo == state.handles[1], "tab index order was not respected");
        require(ui.focusManager().focusedNode() == state.handles[1], "focus manager did not own navigation focus");

        const UiDispatchResult& second = ui.dispatchInput(navFrame(UiNavigationDirection::Next), 2);
        require(second.navigationTo == state.handles[0], "second next did not advance in tab order");

        const UiDispatchResult& previous = ui.dispatchInput(navFrame(UiNavigationDirection::Previous), 3);
        require(previous.navigationTo == state.handles[1], "previous did not move backward in tab order");

        const UiDispatchResult& submit = ui.dispatchInput(submitFrame(), 4);
        require(submit.navigationSubmitted, "submit was not owned by navigation graph");
        require(submit.navigationSubmitTarget == state.handles[1], "submit target was not focused button");
        require(state.pressed[1] == 1, "button widget did not activate from navigation submit");

        const UiEvent* submitEvent = findLastEvent(ui, UiEventType::NavigationSubmit);
        require(submitEvent != nullptr && submitEvent->consumed, "navigation submit event was not consumed by button");
        require(submitEvent != nullptr && submitEvent->defaultPrevented, "navigation submit default was not prevented");
    }

    void testDirectionalMovementAndDisabledSkipping()
    {
        UiSystem ui(nullptr);
        NavProbeState state;
        state.enabled[2] = false;
        state.tabIndex = {0, 1, 2, 3};
        ui.mountComposition(std::make_unique<NavProbeComposition>(state));
        extract(ui);

        ui.dispatchInput(navFrame(UiNavigationDirection::Next), 1);
        require(ui.focusManager().focusedNode() == state.handles[0], "initial focus failed");

        const UiDispatchResult& down = ui.dispatchInput(navFrame(UiNavigationDirection::Down), 2);
        require(down.navigationTo == state.handles[1], "down did not choose vertical neighbor");

        const UiDispatchResult& right = ui.dispatchInput(navFrame(UiNavigationDirection::Right), 3);
        require(right.navigationTo == state.handles[3], "right did not skip disabled spatial neighbor");

        const UiDispatchResult& up = ui.dispatchInput(navFrame(UiNavigationDirection::Up), 4);
        require(up.navigationTo == state.handles[0], "up did not choose nearest available non-disabled candidate");

        const UiDispatchResult& left = ui.dispatchInput(navFrame(UiNavigationDirection::Left), 5);
        require(left.navigationRequestBlocked, "left with no candidate was not blocked");
        require(ui.focusManager().focusedNode() == state.handles[0], "blocked movement changed focus");
    }

    void testHiddenNodesAndWrap()
    {
        UiSystem ui(nullptr);
        NavProbeState state;
        state.visible[1] = false;
        state.wrap = true;
        ui.mountComposition(std::make_unique<NavProbeComposition>(state));
        extract(ui);

        ui.dispatchInput(navFrame(UiNavigationDirection::Next), 1);
        ui.dispatchInput(navFrame(UiNavigationDirection::Next), 2);
        ui.dispatchInput(navFrame(UiNavigationDirection::Next), 3);
        const UiDispatchResult& wrapped = ui.dispatchInput(navFrame(UiNavigationDirection::Next), 4);

        require(wrapped.navigationWrapped, "linear navigation did not report wrap");
        require(wrapped.navigationTo == state.handles[0], "wrap did not return to first visible node");
        require(ui.focusManager().focusedNode() != state.handles[1], "hidden node received focus");
    }

    struct ModalProbeState
    {
        UiHandle outside = UI_INVALID_HANDLE;
        UiHandle modalOwner = UI_INVALID_HANDLE;
        UiHandle inside = UI_INVALID_HANDLE;
    };

    class ModalProbeComposition : public UiComposition
    {
    public:
        explicit ModalProbeComposition(ModalProbeState& inState)
            : state(inState)
        {
        }

        void build(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            context.ui.setLayout(context.root, fillLayout());

            gts::ui::UiButtonDesc outside;
            outside.layout = box(0.10f, 0.10f, 0.20f, 0.10f);
            outside.navigationGroup = "modal";
            outside.tabIndex = 0;
            outsideButton.build(widgetContext, context.root, outside);
            state.outside = outsideButton.root();

            state.modalOwner = context.ui.createNode(context.surface, UiNodeType::Container, context.root);
            context.ui.setLayout(context.surface, state.modalOwner, fillLayout());
            context.ui.setState(context.surface,
                                state.modalOwner,
                                UiStateFlags{.visible = true, .enabled = true, .interactable = false});

            gts::ui::UiButtonDesc inside;
            inside.layout = box(0.50f, 0.10f, 0.20f, 0.10f);
            inside.navigationGroup = "modal";
            inside.tabIndex = 1;
            insideButton.build(widgetContext, state.modalOwner, inside);
            state.inside = insideButton.root();
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            outsideButton.onEvent(widgetContext, event);
            insideButton.onEvent(widgetContext, event);
        }

        void destroy(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            insideButton.destroy(widgetContext);
            outsideButton.destroy(widgetContext);
        }

    private:
        ModalProbeState& state;
        gts::ui::UiButtonWidget outsideButton;
        gts::ui::UiButtonWidget insideButton;
    };

    void testModalRestrictsNavigation()
    {
        UiSystem ui(nullptr);
        ModalProbeState state;
        ui.mountComposition(std::make_unique<ModalProbeComposition>(state));
        extract(ui);

        UiModalDesc modal;
        modal.owner = state.modalOwner;
        modal.initialFocus = state.inside;
        modal.consumeNavigation = true;
        require(ui.pushModal(modal) != UI_INVALID_MODAL, "modal push failed");

        const UiDispatchResult& previous = ui.dispatchInput(navFrame(UiNavigationDirection::Previous), 1);
        require(previous.navigationRequestBlocked, "modal did not block navigation outside owner");
        require(ui.focusManager().focusedNode() == state.inside, "modal navigation escaped owner");
    }

    void testSurfaceIsolation()
    {
        UiSystem ui(nullptr);
        NavProbeState defaultState;
        ui.mountComposition(std::make_unique<NavProbeComposition>(defaultState));

        UiSurfaceDesc surfaceDesc;
        surfaceDesc.name = "secondary";
        surfaceDesc.order = 10;
        surfaceDesc.rect = {0.60f, 0.0f, 0.40f, 1.0f};
        const UiSurfaceId secondary = ui.createSurface(surfaceDesc);

        NavProbeState secondaryState;
        secondaryState.group = "secondary";
        ui.mountComposition(secondary, std::make_unique<NavProbeComposition>(secondaryState));
        ui.extractSurfaceCommandsRef(secondary, 800, 600);
        extract(ui);

        UiSurface* secondarySurface = ui.findSurface(secondary);
        require(secondarySurface != nullptr, "secondary surface missing");
        require(secondarySurface->focusManager().requestFocus(secondarySurface->document(), secondaryState.handles[0]),
                "secondary focus setup failed");
        require(secondarySurface->focusManager().setNavigationFocus(secondarySurface->document(),
                                                                    UI_PRIMARY_INPUT_DEVICE,
                                                                    secondaryState.handles[0]),
                "secondary navigation focus setup failed");

        UiInputFrame next = navFrame(UiNavigationDirection::Next);
        next.pointerX = 0.10f;
        next.pointerY = 0.10f;
        const UiDispatchResult& result = ui.dispatchInput(next, 1);

        require(result.surface == secondary, "navigation request did not route to focused surface");
        require(result.navigationTo == secondaryState.handles[1], "secondary graph did not move locally");
        require(secondarySurface->focusManager().focusedNode() == secondaryState.handles[1],
                "secondary focus did not update");
        require(ui.focusManager().focusedNode() == UI_INVALID_HANDLE, "default surface focus was changed");
    }

    void testDestroyedCompositionPrunesNavigation()
    {
        UiSystem ui(nullptr);
        NavProbeState state;
        const UiCompositionId composition =
            ui.mountComposition(std::make_unique<NavProbeComposition>(state));
        extract(ui);
        ui.dispatchInput(navFrame(UiNavigationDirection::Next), 1);
        require(ui.focusManager().focusedNode() == state.handles[0], "initial focus failed before destroy");

        require(ui.destroyComposition(composition), "composition destroy failed");
        const UiDispatchResult& result = ui.dispatchInput(navFrame(UiNavigationDirection::Next), 2);
        require(result.navigationRequestBlocked, "destroyed graph nodes were not pruned");
        require(ui.focusManager().focusedNode() == UI_INVALID_HANDLE, "destroyed node remained focused");
    }
} // namespace

int main()
{
    testTabOrderAndSubmitActivation();
    testDirectionalMovementAndDisabledSkipping();
    testHiddenNodesAndWrap();
    testModalRestrictsNavigation();
    testSurfaceIsolation();
    testDestroyedCompositionPrunesNavigation();
    return 0;
}
