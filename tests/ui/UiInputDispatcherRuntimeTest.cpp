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
            std::cerr << "UI input dispatcher runtime test failed: " << message << std::endl;
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

    UiHandle createHitRect(UiSystem& ui,
                           UiHandle parent,
                           const UiLayoutSpec& layout,
                           UiColor color = UiColor{1.0f, 1.0f, 1.0f, 1.0f})
    {
        const UiHandle handle = ui.createNode(UiNodeType::Rect, parent);
        ui.setLayout(handle, layout);
        ui.setState(handle, UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        ui.setPayload(handle, UiRectData{color});
        return handle;
    }

    UiInputFrame pointerFrame(float x, float y)
    {
        UiInputFrame frame;
        frame.pointerX = x;
        frame.pointerY = y;
        return frame;
    }

    void testOverlappingSiblingPriority()
    {
        UiSystem ui(nullptr);
        const UiHandle lower = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        const UiHandle upper = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        const UiDispatchResult result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 1);
        require(result.hovered == upper, "later sibling did not win overlapping hit test");
        require(result.hovered != lower, "lower sibling won overlapping hit test");
        require(result.hoveredLayer == ui.getDefaultLayer(), "sibling hit did not report default layer");
        require(result.pointerConsumed, "hovered sibling did not consume pointer");
    }

    void testLayerPriorityAndLayerState()
    {
        UiSystem ui(nullptr);
        const UiHandle base = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        const UiLayerId overlayLayer = ui.createLayer("overlay", 100);
        const UiHandle overlayRoot = ui.getLayerRoot(overlayLayer);
        const UiHandle overlay = createHitRect(ui, overlayRoot, rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        UiDispatchResult result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 1);
        require(result.hovered == overlay, "higher layer did not own hover");
        require(result.hoveredLayer == overlayLayer, "higher layer hover reported wrong layer");

        require(ui.setLayerOrder(overlayLayer, -100), "overlay layer order change failed");
        result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 2);
        require(result.hovered == base, "lower ordered layer still won hover");
        require(result.hoveredLayer == ui.getDefaultLayer(), "base hover reported wrong layer");

        require(ui.setLayerOrder(overlayLayer, 100), "overlay layer order restore failed");
        require(ui.setLayerState(overlayLayer, UiLayerState{.visible = true, .inputEnabled = false}),
                "overlay input disable failed");
        result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 3);
        require(result.hovered == base, "input-disabled layer owned hover");

        require(ui.setLayerState(overlayLayer, UiLayerState{.visible = false, .inputEnabled = true}),
                "overlay hide failed");
        result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 4);
        require(result.hovered == base, "hidden layer owned hover");
    }

    void testHoverChangesUpdateNodeState()
    {
        UiSystem ui(nullptr);
        const UiHandle left = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.5f, 1.0f));
        const UiHandle right = createHitRect(ui, ui.getRoot(), rectLayout(0.5f, 0.0f, 0.5f, 1.0f));

        UiDispatchResult result = ui.dispatchInput(pointerFrame(0.25f, 0.5f), 1);
        require(result.hovered == left, "left hover did not resolve");
        require(ui.findNode(left)->state.hovered, "left node state was not hovered");

        result = ui.dispatchInput(pointerFrame(0.75f, 0.5f), 2);
        require(result.hovered == right, "right hover did not resolve");
        require(!ui.findNode(left)->state.hovered, "left node remained hovered after hover moved");
        require(ui.findNode(right)->state.hovered, "right node state was not hovered");
    }

    void testClickAndReleaseOwnership()
    {
        UiSystem ui(nullptr);
        const UiHandle button = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        UiInputFrame press = pointerFrame(0.5f, 0.5f);
        press.primaryDown = true;
        press.primaryPressed = true;
        UiDispatchResult result = ui.dispatchInput(press, 1);
        require(result.active == button, "press did not assign active handle");
        require(result.pressed == button, "press did not assign pressed handle");
        require(result.clicked == UI_INVALID_HANDLE, "press incorrectly clicked");
        require(result.activeLayer == ui.getDefaultLayer(), "active handle reported wrong layer");

        UiInputFrame release = pointerFrame(0.5f, 0.5f);
        release.primaryReleased = true;
        result = ui.dispatchInput(release, 2);
        require(result.released == button, "release did not report released handle");
        require(result.clicked == button, "release on active handle did not click");
        require(result.active == UI_INVALID_HANDLE, "active handle survived release");
        require(result.clickedLayer == ui.getDefaultLayer(), "clicked handle reported wrong layer");
    }

    void testReleaseAwayDoesNotClick()
    {
        UiSystem ui(nullptr);
        const UiHandle left = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.5f, 1.0f));
        const UiHandle right = createHitRect(ui, ui.getRoot(), rectLayout(0.5f, 0.0f, 0.5f, 1.0f));

        UiInputFrame press = pointerFrame(0.25f, 0.5f);
        press.primaryDown = true;
        press.primaryPressed = true;
        ui.dispatchInput(press, 1);

        UiInputFrame release = pointerFrame(0.75f, 0.5f);
        release.primaryReleased = true;
        const UiDispatchResult result = ui.dispatchInput(release, 2);
        require(result.hovered == right, "release-away hover did not move to right node");
        require(result.released == left, "release-away did not report original active handle");
        require(result.clicked == UI_INVALID_HANDLE, "release-away incorrectly clicked");
    }

    void testDispatchOncePerFrame()
    {
        UiSystem ui(nullptr);
        const UiHandle left = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.5f, 1.0f));
        const UiHandle right = createHitRect(ui, ui.getRoot(), rectLayout(0.5f, 0.0f, 0.5f, 1.0f));

        const UiDispatchResult first = ui.dispatchInput(pointerFrame(0.25f, 0.5f), 42);
        const UiDispatchResult sameFrame = ui.dispatchInput(pointerFrame(0.75f, 0.5f), 42);
        require(first.dispatchSequence == sameFrame.dispatchSequence, "same frame dispatched twice");
        require(sameFrame.hovered == left, "same frame dispatch did not return cached result");

        const UiDispatchResult nextFrame = ui.dispatchInput(pointerFrame(0.75f, 0.5f), 43);
        require(nextFrame.dispatchSequence == first.dispatchSequence + 1, "next frame did not dispatch once");
        require(nextFrame.hovered == right, "next frame dispatch did not update hover");
    }
}

int main()
{
    testOverlappingSiblingPriority();
    testLayerPriorityAndLayerState();
    testHoverChangesUpdateNodeState();
    testClickAndReleaseOwnership();
    testReleaseAwayDoesNotClick();
    testDispatchOncePerFrame();
    return 0;
}
