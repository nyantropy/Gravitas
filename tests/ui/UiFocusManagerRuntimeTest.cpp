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
            std::cerr << "UI focus manager runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    UiLayoutSpec rectLayout(float x, float y, float width, float height)
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Absolute;
        layout.widthMode    = UiSizeMode::Fixed;
        layout.heightMode   = UiSizeMode::Fixed;
        layout.offsetMin    = {x, y};
        layout.fixedWidth   = width;
        layout.fixedHeight  = height;
        return layout;
    }

    UiHandle createNode(UiSystem& ui, UiHandle parent, const UiLayoutSpec& layout)
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

    void testHoverTransitions()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus = ui.focusManager();
        const UiHandle  left  = createNode(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.5f, 1.0f));
        const UiHandle  right = createNode(ui, ui.getRoot(), rectLayout(0.5f, 0.0f, 0.5f, 1.0f));

        require(focus.setHovered(ui.getDocument(), UI_PRIMARY_POINTER, left), "left hover failed");
        require(focus.hoveredNode() == left, "left was not tracked as hovered");
        require(ui.findNode(left)->state.hovered, "left hover flag was not reflected");

        require(focus.setHovered(ui.getDocument(), UI_PRIMARY_POINTER, right), "right hover failed");
        require(focus.hoveredNode() == right, "right was not tracked as hovered");
        require(!ui.findNode(left)->state.hovered, "left hover flag was not cleared");
        require(ui.findNode(right)->state.hovered, "right hover flag was not reflected");
    }

    void testFocusTransitions()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus  = ui.focusManager();
        const UiHandle  first  = createNode(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.5f, 1.0f));
        const UiHandle  second = createNode(ui, ui.getRoot(), rectLayout(0.5f, 0.0f, 0.5f, 1.0f));

        require(focus.requestFocus(ui.getDocument(), first), "first focus failed");
        require(focus.focusedNode() == first, "first was not focused");
        require(ui.findNode(first)->state.focused, "first focus flag was not reflected");

        require(focus.requestFocus(ui.getDocument(), second), "second focus failed");
        require(focus.focusedNode() == second, "second was not focused");
        require(!ui.findNode(first)->state.focused, "first focus flag was not cleared");
        require(ui.findNode(second)->state.focused, "second focus flag was not reflected");

        require(focus.restoreFocus(ui.getDocument()), "restore focus failed");
        require(focus.focusedNode() == first, "restore did not return to previous focus");
    }

    void testPointerCaptureAndRelease()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus  = ui.focusManager();
        const UiHandle  target = createNode(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        require(focus.capturePointer(ui.getDocument(), UI_PRIMARY_POINTER, target), "capture failed");
        require(focus.capturedNode() == target, "capture owner was not tracked");
        require(focus.releasePointer(UI_PRIMARY_POINTER), "capture release failed");
        require(focus.capturedNode() == UI_INVALID_HANDLE, "capture owner survived release");
    }

    void testCaptureSurvivesHoverChanges()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus    = ui.focusManager();
        const UiHandle  captured = createNode(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.5f, 1.0f));
        const UiHandle  hovered  = createNode(ui, ui.getRoot(), rectLayout(0.5f, 0.0f, 0.5f, 1.0f));

        require(focus.capturePointer(ui.getDocument(), UI_PRIMARY_POINTER, captured), "capture failed");
        require(focus.setHovered(ui.getDocument(), UI_PRIMARY_POINTER, hovered), "hover change failed");
        require(focus.capturedNode() == captured, "capture changed when hover moved");
        require(focus.hoveredNode() == hovered, "hover did not move independently from capture");
        require(!ui.findNode(captured)->state.hovered, "captured node incorrectly became hovered");
        require(ui.findNode(hovered)->state.hovered, "hovered node flag was not reflected");
    }

    void testActivePointerReflection()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus  = ui.focusManager();
        const UiHandle  target = createNode(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        require(focus.setActivePointer(ui.getDocument(), UI_PRIMARY_POINTER, target), "active pointer failed");
        require(focus.activePointerNode() == target, "active pointer was not tracked");
        require(ui.findNode(target)->state.pressed, "active pointer did not reflect pressed state");

        require(focus.clearActivePointer(ui.getDocument(), UI_PRIMARY_POINTER), "active pointer clear failed");
        require(focus.activePointerNode() == UI_INVALID_HANDLE, "active pointer survived clear");
        require(!ui.findNode(target)->state.pressed, "pressed state survived active clear");
    }

    void testNestedScopesRestoreFocus()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus          = ui.focusManager();
        const UiHandle  rootButton     = createNode(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.3f, 1.0f));
        const UiHandle  settingsButton = createNode(ui, ui.getRoot(), rectLayout(0.3f, 0.0f, 0.3f, 1.0f));
        const UiHandle  videoButton    = createNode(ui, ui.getRoot(), rectLayout(0.6f, 0.0f, 0.3f, 1.0f));

        require(focus.requestFocus(ui.getDocument(), rootButton), "root focus failed");
        const UiFocusScopeId settingsScope = focus.pushScope(settingsButton);
        require(focus.requestFocus(ui.getDocument(), settingsButton), "settings focus failed");
        const UiFocusScopeId videoScope = focus.pushScope(videoButton);
        require(focus.requestFocus(ui.getDocument(), videoButton), "video focus failed");

        require(focus.popScope(ui.getDocument(), videoScope), "video scope pop failed");
        require(focus.focusedNode() == settingsButton, "video scope did not restore settings focus");
        require(focus.popScope(ui.getDocument(), settingsScope), "settings scope pop failed");
        require(focus.focusedNode() == rootButton, "settings scope did not restore root focus");
    }

    void testDisabledAndHiddenNodeFocus()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus    = ui.focusManager();
        const UiHandle  valid    = createNode(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.3f, 1.0f));
        const UiHandle  disabled = createNode(ui, ui.getRoot(), rectLayout(0.3f, 0.0f, 0.3f, 1.0f));
        const UiHandle  hidden   = createNode(ui, ui.getRoot(), rectLayout(0.6f, 0.0f, 0.3f, 1.0f));

        require(focus.requestFocus(ui.getDocument(), valid), "valid focus failed");

        UiStateFlags disabledState = ui.findNode(disabled)->state;
        disabledState.enabled      = false;
        ui.setState(disabled, disabledState);
        require(!focus.requestFocus(ui.getDocument(), disabled), "disabled node accepted focus");
        require(focus.focusedNode() == valid, "disabled focus request changed current focus");

        UiStateFlags hiddenState = ui.findNode(hidden)->state;
        hiddenState.visible      = false;
        ui.setState(hidden, hiddenState);
        require(!focus.requestFocus(ui.getDocument(), hidden), "hidden node accepted focus");
        require(focus.focusedNode() == valid, "hidden focus request changed current focus");
    }

    void testDestroyedNodePrunesFocus()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus  = ui.focusManager();
        const UiHandle  target = createNode(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        require(focus.requestFocus(ui.getDocument(), target), "focus failed");
        require(focus.capturePointer(ui.getDocument(), UI_PRIMARY_POINTER, target), "capture failed");
        require(focus.setActivePointer(ui.getDocument(), UI_PRIMARY_POINTER, target), "active failed");
        require(ui.removeNode(target), "node removal failed");

        require(focus.focusedNode() == UI_INVALID_HANDLE, "destroyed node kept focus");
        require(focus.capturedNode() == UI_INVALID_HANDLE, "destroyed node kept capture");
        require(focus.activePointerNode() == UI_INVALID_HANDLE, "destroyed node kept active pointer");
    }

    void testLayerFocusInteractions()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus         = ui.focusManager();
        const UiLayerId overlayLayer  = ui.createLayer("overlay", 100);
        const UiHandle  overlayRoot   = ui.getLayerRoot(overlayLayer);
        const UiHandle  overlayButton = createNode(ui, overlayRoot, rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        require(focus.requestFocus(ui.getDocument(), overlayButton), "visible layer focus failed");
        require(focus.focusedNode() == overlayButton, "visible layer focus was not tracked");

        require(ui.setLayerState(overlayLayer, UiLayerState{.visible = true, .inputEnabled = false}),
                "layer input disable failed");
        require(focus.focusedNode() == UI_INVALID_HANDLE, "input-disabled layer kept focus");
        require(!focus.requestFocus(ui.getDocument(), overlayButton), "input-disabled layer accepted focus");

        require(ui.setLayerState(overlayLayer, UiLayerState{.visible = false, .inputEnabled = true}),
                "layer hide failed");
        require(!focus.requestFocus(ui.getDocument(), overlayButton), "hidden layer accepted focus");
    }

    void testIndependentTextAndNavigationFocus()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus      = ui.focusManager();
        const UiHandle  keyboard   = createNode(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.3f, 1.0f));
        const UiHandle  text       = createNode(ui, ui.getRoot(), rectLayout(0.3f, 0.0f, 0.3f, 1.0f));
        const UiHandle  navigation = createNode(ui, ui.getRoot(), rectLayout(0.6f, 0.0f, 0.3f, 1.0f));

        require(focus.requestFocus(ui.getDocument(), keyboard), "keyboard focus failed");
        require(focus.requestTextInputFocus(ui.getDocument(), text), "text input focus failed");
        require(focus.setNavigationFocus(ui.getDocument(), UI_PRIMARY_INPUT_DEVICE, navigation),
                "navigation focus failed");

        require(focus.keyboardFocusedNode() == keyboard, "keyboard focus was not independent");
        require(focus.textInputFocusedNode() == text, "text input focus was not tracked");
        require(focus.navigationFocusedNode() == navigation, "navigation focus was not tracked");
    }

    void testDispatchUsesCapturedPointerTarget()
    {
        UiSystem        ui(nullptr);
        UiFocusManager& focus = ui.focusManager();
        const UiHandle  left  = createNode(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.5f, 1.0f));
        const UiHandle  right = createNode(ui, ui.getRoot(), rectLayout(0.5f, 0.0f, 0.5f, 1.0f));

        require(focus.capturePointer(ui.getDocument(), UI_PRIMARY_POINTER, left), "capture failed");

        UiInputFrame press      = pointerFrame(0.75f, 0.5f);
        press.primaryDown       = true;
        press.primaryPressed    = true;
        UiDispatchResult result = ui.dispatchInput(press, 1);

        require(result.hovered == right, "capture changed physical hover");
        require(result.captured == left, "dispatch did not report captured owner");
        require(result.active == left, "capture did not own pointer press");
        require(result.pressed == left, "capture did not own pressed node");
        require(result.pointerConsumed, "captured pointer did not consume pointer input");

        UiInputFrame release    = pointerFrame(0.75f, 0.5f);
        release.primaryReleased = true;
        result                  = ui.dispatchInput(release, 2);
        require(result.released == left, "capture did not own pointer release");
        require(result.clicked == left, "capture release did not click captured owner");
    }
} // namespace

int main()
{
    testHoverTransitions();
    testFocusTransitions();
    testPointerCaptureAndRelease();
    testCaptureSurvivesHoverChanges();
    testActivePointerReflection();
    testNestedScopesRestoreFocus();
    testDisabledAndHiddenNodeFocus();
    testDestroyedNodePrunesFocus();
    testLayerFocusInteractions();
    testIndependentTextAndNavigationFocus();
    testDispatchUsesCapturedPointerTarget();
    return 0;
}
