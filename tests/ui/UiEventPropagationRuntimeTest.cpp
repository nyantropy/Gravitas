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
        UiHandle target = UI_INVALID_HANDLE;
        UiHandle currentTarget = UI_INVALID_HANDLE;
        UiCompositionId currentComposition = UI_INVALID_COMPOSITION;
        bool consumed = false;
        bool defaultPrevented = false;
    };

    struct ProbeState
    {
        std::vector<EventRecord> records;
        UiHandle button = UI_INVALID_HANDLE;
        UiHandle secondButton = UI_INVALID_HANDLE;
        bool consumeCapture = false;
        bool preventTargetDefault = false;
    };

    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI event propagation runtime test failed: " << message << std::endl;
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

    class ProbeComposition : public UiComposition
    {
    public:
        explicit ProbeComposition(ProbeState& inState, bool inTwoButtons = false)
            : state(inState),
              twoButtons(inTwoButtons)
        {
        }

        void build(UiCompositionContext& context) override
        {
            state.button = createHitRect(context.ui, context.root, rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
            if (twoButtons)
            {
                context.ui.setLayout(state.button, rectLayout(0.0f, 0.0f, 0.5f, 1.0f));
                state.secondButton = createHitRect(context.ui, context.root, rectLayout(0.5f, 0.0f, 0.5f, 1.0f));
            }
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            state.records.push_back(EventRecord{
                event.type,
                event.phase,
                event.target,
                event.currentTarget,
                event.currentComposition,
                event.consumed,
                event.defaultPrevented
            });

            if (state.consumeCapture &&
                event.type == UiEventType::PointerClick &&
                event.phase == UiEventPhase::Capture &&
                event.currentTarget == context.root)
            {
                event.consume();
                return;
            }

            if (state.preventTargetDefault &&
                event.type == UiEventType::PointerClick &&
                event.phase == UiEventPhase::Target)
            {
                event.preventDefault();
            }
        }

        void destroy(UiCompositionContext& context) override
        {
            if (state.button != UI_INVALID_HANDLE)
                context.ui.removeNode(state.button);
            if (state.secondButton != UI_INVALID_HANDLE)
                context.ui.removeNode(state.secondButton);
            state.button = UI_INVALID_HANDLE;
            state.secondButton = UI_INVALID_HANDLE;
        }

    private:
        ProbeState& state;
        bool twoButtons = false;
    };

    UiMountDesc childMount(UiMountId parentMount)
    {
        UiMountDesc desc;
        desc.attachment.parentMount = parentMount;
        return desc;
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

    std::vector<EventRecord> recordsOfType(const ProbeState& state, UiEventType type)
    {
        std::vector<EventRecord> result;
        for (const EventRecord& record : state.records)
        {
            if (record.type == type)
                result.push_back(record);
        }
        return result;
    }

    void dispatchClick(UiSystem& ui, float x = 0.5f, float y = 0.5f)
    {
        ui.dispatchInput(pointerFrame(x, y), 1);
        ui.dispatchInput(pressFrame(x, y), 2);
        ui.dispatchInput(releaseFrame(x, y), 3);
    }

    void testPointerEnterLeave()
    {
        UiSystem ui(nullptr);
        ProbeState state;
        ui.mountComposition(std::make_unique<ProbeComposition>(state));

        ui.dispatchInput(pointerFrame(0.5f, 0.5f), 1);
        require(!recordsOfType(state, UiEventType::PointerEnter).empty(), "pointer enter was not delivered");

        state.records.clear();
        ui.dispatchInput(pointerFrame(2.0f, 2.0f), 2);
        require(!recordsOfType(state, UiEventType::PointerLeave).empty(), "pointer leave was not delivered");
    }

    void testClickCaptureTargetBubble()
    {
        UiSystem ui(nullptr);
        ProbeState state;
        const UiCompositionId composition =
            ui.mountComposition(std::make_unique<ProbeComposition>(state));
        state.records.clear();

        dispatchClick(ui);
        const std::vector<EventRecord> clickRecords = recordsOfType(state, UiEventType::PointerClick);
        require(clickRecords.size() >= 3, "click did not propagate through all phases");
        require(clickRecords.front().phase == UiEventPhase::Capture, "click did not start in capture phase");
        require(clickRecords.front().currentComposition == composition, "capture did not target composition");

        bool sawTarget = false;
        bool sawBubble = false;
        for (const EventRecord& record : clickRecords)
        {
            if (record.phase == UiEventPhase::Target && record.currentTarget == state.button)
                sawTarget = true;
            if (sawTarget && record.phase == UiEventPhase::Bubble)
                sawBubble = true;
        }
        require(sawTarget, "click did not reach target phase");
        require(sawBubble, "click did not bubble after target");
    }

    void testConsumptionStopsPropagation()
    {
        UiSystem ui(nullptr);
        ProbeState state;
        state.consumeCapture = true;
        ui.mountComposition(std::make_unique<ProbeComposition>(state));
        state.records.clear();

        dispatchClick(ui);
        const std::vector<EventRecord> clickRecords = recordsOfType(state, UiEventType::PointerClick);
        require(clickRecords.size() == 1, "consumed click propagated past capture");
        require(clickRecords.front().phase == UiEventPhase::Capture, "click was not consumed during capture");

        const UiEvent* event = findLastEvent(ui, UiEventType::PointerClick);
        require(event != nullptr && event->consumed, "final click event was not marked consumed");
    }

    void testDefaultPreventionDoesNotStopPropagation()
    {
        UiSystem ui(nullptr);
        ProbeState state;
        state.preventTargetDefault = true;
        ui.mountComposition(std::make_unique<ProbeComposition>(state));
        state.records.clear();

        dispatchClick(ui);
        const std::vector<EventRecord> clickRecords = recordsOfType(state, UiEventType::PointerClick);
        bool sawBubbleAfterPreventDefault = false;
        for (const EventRecord& record : clickRecords)
        {
            if (record.phase == UiEventPhase::Bubble)
                sawBubbleAfterPreventDefault = true;
        }

        const UiEvent* event = findLastEvent(ui, UiEventType::PointerClick);
        require(event != nullptr && event->defaultPrevented, "final click event did not preserve default prevention");
        require(sawBubbleAfterPreventDefault, "preventDefault stopped propagation");
    }

    void testNestedCompositions()
    {
        UiSystem ui(nullptr);
        ProbeState parentState;
        ProbeState childState;
        const UiCompositionId parentComposition =
            ui.mountComposition(std::make_unique<ProbeComposition>(parentState));
        const UiMountId parentMount = ui.compositionMount(parentComposition);
        const UiCompositionId childComposition =
            ui.mountComposition(std::make_unique<ProbeComposition>(childState), childMount(parentMount));

        parentState.records.clear();
        childState.records.clear();
        dispatchClick(ui);

        const std::vector<EventRecord> parentClicks = recordsOfType(parentState, UiEventType::PointerClick);
        const std::vector<EventRecord> childClicks = recordsOfType(childState, UiEventType::PointerClick);
        require(!parentClicks.empty(), "parent composition did not receive nested click");
        require(!childClicks.empty(), "child composition did not receive nested click");
        require(childClicks.front().currentComposition == childComposition, "child click used wrong composition id");
    }

    void testModalRoutingAndCancel()
    {
        UiSystem ui(nullptr);
        ProbeState backgroundState;
        ProbeState modalState;
        const UiLayerId backgroundLayer = ui.createLayer("background", 0);
        const UiLayerId modalLayer = ui.createLayer("modal", 100);

        UiMountDesc backgroundDesc;
        backgroundDesc.attachment.layer = backgroundLayer;
        ui.mountComposition(std::make_unique<ProbeComposition>(backgroundState), backgroundDesc);

        UiMountDesc modalDesc;
        modalDesc.attachment.layer = modalLayer;
        ui.mountComposition(std::make_unique<ProbeComposition>(modalState), modalDesc);

        UiModalDesc modal;
        modal.layer = modalLayer;
        modal.owner = modalState.button;
        modal.initialFocus = modalState.button;
        modal.dismissOnCancel = false;
        require(ui.pushModal(modal) != UI_INVALID_MODAL, "modal push failed");

        backgroundState.records.clear();
        modalState.records.clear();
        dispatchClick(ui);
        require(recordsOfType(backgroundState, UiEventType::PointerClick).empty(),
                "blocked background received modal click");
        require(!recordsOfType(modalState, UiEventType::PointerClick).empty(),
                "modal owner did not receive click");

        modalState.records.clear();
        UiInputFrame cancel = pointerFrame(0.5f, 0.5f);
        cancel.cancelPressed = true;
        ui.dispatchInput(cancel, 4);
        require(!recordsOfType(modalState, UiEventType::NavigationCancel).empty(),
                "modal owner did not receive navigation cancel");
    }

    void testFocusEvents()
    {
        UiSystem ui(nullptr);
        ProbeState state;
        ui.mountComposition(std::make_unique<ProbeComposition>(state, true));

        ui.dispatchInput(pointerFrame(0.25f, 0.5f), 1);
        ui.dispatchInput(pressFrame(0.25f, 0.5f), 2);
        require(!recordsOfType(state, UiEventType::FocusGained).empty(), "focus gained was not delivered");

        state.records.clear();
        ui.focusManager().clearFocus(ui.getDocument());
        ui.dispatchInput(pointerFrame(0.75f, 0.5f), 3);
        require(!recordsOfType(state, UiEventType::FocusLost).empty(), "focus lost was not delivered");
    }

    void testDestroyedTargetsAreIgnored()
    {
        UiSystem ui(nullptr);
        ProbeState state;
        const UiCompositionId composition =
            ui.mountComposition(std::make_unique<ProbeComposition>(state));
        const UiHandle oldTarget = state.button;

        require(ui.destroyComposition(composition), "composition destroy failed");

        UiEvent event;
        event.type = UiEventType::PointerClick;
        event.target = oldTarget;
        event.pointerX = 0.5f;
        event.pointerY = 0.5f;
        require(!ui.propagateEvent(event), "destroyed event target was delivered");
        require(!ui.events().empty() && ui.events().back().target == UI_INVALID_HANDLE,
                "destroyed event target was not invalidated");
    }
}

int main()
{
    testPointerEnterLeave();
    testClickCaptureTargetBubble();
    testConsumptionStopsPropagation();
    testDefaultPreventionDoesNotStopPropagation();
    testNestedCompositions();
    testModalRoutingAndCancel();
    testFocusEvents();
    testDestroyedTargetsAreIgnored();
    return 0;
}
