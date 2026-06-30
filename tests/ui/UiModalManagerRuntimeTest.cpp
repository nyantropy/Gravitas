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
            std::cerr << "UI modal manager runtime test failed: " << message << std::endl;
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

    UiModalDesc modalDesc(UiLayerId layer, UiHandle owner, UiHandle initialFocus)
    {
        UiModalDesc desc;
        desc.layer = layer;
        desc.owner = owner;
        desc.initialFocus = initialFocus;
        return desc;
    }

    void testSingleModalPublishesState()
    {
        UiSystem ui(nullptr);
        const UiLayerId modalLayer = ui.createLayer("modal", 100);
        const UiHandle owner = createHitRect(ui, ui.getLayerRoot(modalLayer), rectLayout(0.25f, 0.25f, 0.5f, 0.5f));

        const UiModalId modal = ui.pushModal(modalDesc(modalLayer, owner, owner));
        require(modal != UI_INVALID_MODAL, "modal push failed");
        require(ui.modalManager().depth() == 1, "modal depth was not one");
        require(ui.modalManager().topModalId() == modal, "top modal id was not tracked");
        require(ui.focusManager().focusedNode() == owner, "modal did not request focus");

        const UiDispatchResult result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 1);
        require(result.modalActive, "dispatch did not report active modal");
        require(result.modalDepth == 1, "dispatch reported wrong modal depth");
        require(result.modal == modal, "dispatch reported wrong modal id");
        require(result.modalOwner == owner, "dispatch reported wrong modal owner");
        require(result.modalLayer == modalLayer, "dispatch reported wrong modal layer");
        require(result.hovered == owner, "modal layer did not receive pointer hit");
    }

    void testNestedModalsAndStackIntegrity()
    {
        UiSystem ui(nullptr);
        const UiLayerId firstLayer = ui.createLayer("first-modal", 100);
        const UiLayerId secondLayer = ui.createLayer("second-modal", 200);
        const UiHandle first = createHitRect(ui, ui.getLayerRoot(firstLayer), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        const UiHandle second = createHitRect(ui, ui.getLayerRoot(secondLayer), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        const UiModalId firstModal = ui.pushModal(modalDesc(firstLayer, first, first));
        const UiModalId secondModal = ui.pushModal(modalDesc(secondLayer, second, second));

        require(ui.modalManager().depth() == 2, "nested modal depth was not two");
        require(ui.modalManager().topModalId() == secondModal, "nested top modal was not newest modal");
        require(!ui.popModal(firstModal), "non-top modal popped out of order");
        require(ui.modalManager().topModalId() == secondModal, "failed pop changed stack top");

        require(ui.popModal(secondModal), "top nested modal pop failed");
        require(ui.modalManager().depth() == 1, "nested modal pop did not reveal parent");
        require(ui.modalManager().topModalId() == firstModal, "parent modal was not restored as top");
    }

    void testFocusRestoration()
    {
        UiSystem ui(nullptr);
        const UiHandle base = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 0.25f, 1.0f));
        require(ui.focusManager().requestFocus(ui.getDocument(), base), "base focus failed");

        const UiLayerId firstLayer = ui.createLayer("settings", 100);
        const UiLayerId secondLayer = ui.createLayer("confirm", 200);
        const UiHandle first = createHitRect(ui, ui.getLayerRoot(firstLayer), rectLayout(0.25f, 0.0f, 0.25f, 1.0f));
        const UiHandle second = createHitRect(ui, ui.getLayerRoot(secondLayer), rectLayout(0.5f, 0.0f, 0.25f, 1.0f));

        const UiModalId firstModal = ui.pushModal(modalDesc(firstLayer, first, first));
        const UiModalId secondModal = ui.pushModal(modalDesc(secondLayer, second, second));
        require(ui.focusManager().focusedNode() == second, "nested modal did not take focus");

        require(ui.popModal(secondModal), "confirm pop failed");
        require(ui.focusManager().focusedNode() == first, "parent modal focus was not restored");

        require(ui.popModal(firstModal), "settings pop failed");
        require(ui.focusManager().focusedNode() == base, "base focus was not restored");
    }

    void testPointerBlockingAndLayerBlocking()
    {
        UiSystem ui(nullptr);
        const UiHandle base = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        const UiLayerId modalLayer = ui.createLayer("modal", 100);
        const UiHandle modalPanel =
            createHitRect(ui, ui.getLayerRoot(modalLayer), rectLayout(0.35f, 0.35f, 0.3f, 0.3f));

        const UiModalId modal = ui.pushModal(modalDesc(modalLayer, modalPanel, modalPanel));
        require(modal != UI_INVALID_MODAL, "modal push failed");

        UiDispatchResult result = ui.dispatchInput(pointerFrame(0.1f, 0.1f), 1);
        require(result.hovered == UI_INVALID_HANDLE, "blocked lower layer received hover");
        require(result.hovered != base, "base layer won hit under modal");
        require(result.pointerBlocked, "modal did not report pointer blocking");
        require(result.pointerConsumed, "blocked modal pointer did not consume pointer input");

        result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 2);
        require(result.hovered == modalPanel, "modal panel did not receive pointer inside modal layer");
    }

    void testBlockingFlags()
    {
        UiSystem ui(nullptr);
        const UiLayerId modalLayer = ui.createLayer("modal", 100);
        const UiHandle owner = createHitRect(ui, ui.getLayerRoot(modalLayer), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        UiModalDesc desc = modalDesc(modalLayer, owner, owner);
        desc.consumeKeyboard = true;
        desc.consumeNavigation = true;
        desc.consumeTextInput = true;
        ui.pushModal(desc);

        const UiDispatchResult result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 1);
        require(result.keyboardBlocked, "modal did not block keyboard");
        require(result.navigationBlocked, "modal did not block navigation");
        require(result.textBlocked, "modal did not block text input");
        require(result.keyboardConsumed, "keyboard block did not consume keyboard");
        require(result.navigationConsumed, "navigation block did not consume navigation");
        require(result.textInputConsumed, "text block did not consume text input");
    }

    void testCancelDismissesTopModalOnly()
    {
        UiSystem ui(nullptr);
        const UiLayerId firstLayer = ui.createLayer("pause", 100);
        const UiLayerId secondLayer = ui.createLayer("confirm", 200);
        const UiHandle first = createHitRect(ui, ui.getLayerRoot(firstLayer), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        const UiHandle second = createHitRect(ui, ui.getLayerRoot(secondLayer), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        const UiModalId firstModal = ui.pushModal(modalDesc(firstLayer, first, first));
        const UiModalId secondModal = ui.pushModal(modalDesc(secondLayer, second, second));

        UiInputFrame cancel;
        cancel.cancelPressed = true;
        const UiDispatchResult result = ui.dispatchInput(cancel, 1);
        require(result.cancelConsumed, "cancel was not consumed by modal");
        require(result.cancelTargetModal == secondModal, "cancel did not route to top modal");
        require(result.dismissedModal == secondModal, "cancel did not dismiss top modal");
        require(result.modalActive, "parent modal was not active after nested cancel");
        require(result.modal == firstModal, "cancel dismissed more than the top modal");
        require(result.modalDepth == 1, "modal depth after cancel was wrong");
    }

    void testManualDismiss()
    {
        UiSystem ui(nullptr);
        const UiLayerId modalLayer = ui.createLayer("modal", 100);
        const UiHandle owner = createHitRect(ui, ui.getLayerRoot(modalLayer), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        const UiModalId modal = ui.pushModal(modalDesc(modalLayer, owner, owner));

        require(ui.dismissTopModal(), "manual dismiss failed");
        require(!ui.modalManager().hasModal(), "manual dismiss left modal active");
        require(!ui.popModal(modal), "dismissed modal popped again");
    }

    void testHiddenAndDisabledLayerCleanup()
    {
        UiSystem ui(nullptr);
        const UiHandle base = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        const UiLayerId modalLayer = ui.createLayer("modal", 100);
        const UiHandle owner = createHitRect(ui, ui.getLayerRoot(modalLayer), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));

        ui.pushModal(modalDesc(modalLayer, owner, owner));
        require(ui.setLayerState(modalLayer, UiLayerState{.visible = false, .inputEnabled = true}),
                "hide modal layer failed");
        require(!ui.modalManager().hasModal(), "hidden modal layer kept modal active");
        UiDispatchResult result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 1);
        require(result.hovered == base, "base did not receive hover after hidden modal cleanup");

        require(ui.setLayerState(modalLayer, UiLayerState{.visible = true, .inputEnabled = true}),
                "show modal layer failed");
        ui.pushModal(modalDesc(modalLayer, owner, owner));
        require(ui.setLayerState(modalLayer, UiLayerState{.visible = true, .inputEnabled = false}),
                "disable modal layer failed");
        require(!ui.modalManager().hasModal(), "disabled modal layer kept modal active");
        result = ui.dispatchInput(pointerFrame(0.5f, 0.5f), 2);
        require(result.hovered == base, "base did not receive hover after disabled modal cleanup");
    }

    void testDestroyedModalCleanup()
    {
        UiSystem ui(nullptr);
        const UiHandle base = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        const UiLayerId modalLayer = ui.createLayer("modal", 100);
        const UiHandle owner = createHitRect(ui, ui.getLayerRoot(modalLayer), rectLayout(0.25f, 0.25f, 0.5f, 0.5f));

        ui.pushModal(modalDesc(modalLayer, owner, owner));
        require(ui.removeNode(owner), "owner node removal failed");
        require(!ui.modalManager().hasModal(), "destroyed owner kept modal active");

        const UiDispatchResult result = ui.dispatchInput(pointerFrame(0.1f, 0.1f), 1);
        require(!result.modalActive, "dispatch reported destroyed modal");
        require(result.hovered == base, "base did not receive hover after destroyed modal cleanup");
    }

    void testHiddenAndDisabledOwnerCleanup()
    {
        UiSystem ui(nullptr);
        const UiHandle base = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        const UiLayerId modalLayer = ui.createLayer("modal", 100);
        const UiHandle owner = createHitRect(ui, ui.getLayerRoot(modalLayer), rectLayout(0.25f, 0.25f, 0.5f, 0.5f));

        ui.pushModal(modalDesc(modalLayer, owner, owner));
        UiStateFlags hidden = ui.findNode(owner)->state;
        hidden.visible = false;
        require(ui.setState(owner, hidden), "hiding modal owner failed");
        require(!ui.modalManager().hasModal(), "hidden modal owner kept modal active");
        UiDispatchResult result = ui.dispatchInput(pointerFrame(0.1f, 0.1f), 1);
        require(result.hovered == base, "base did not receive hover after hidden modal owner cleanup");

        hidden.visible = true;
        require(ui.setState(owner, hidden), "showing modal owner failed");
        ui.pushModal(modalDesc(modalLayer, owner, owner));
        UiStateFlags disabled = ui.findNode(owner)->state;
        disabled.enabled = false;
        require(ui.setState(owner, disabled), "disabling modal owner failed");
        require(!ui.modalManager().hasModal(), "disabled modal owner kept modal active");
        result = ui.dispatchInput(pointerFrame(0.1f, 0.1f), 2);
        require(result.hovered == base, "base did not receive hover after disabled modal owner cleanup");
    }

    void testNonBlockingPopupPolicy()
    {
        UiSystem ui(nullptr);
        const UiHandle base = createHitRect(ui, ui.getRoot(), rectLayout(0.0f, 0.0f, 1.0f, 1.0f));
        const UiLayerId popupLayer = ui.createLayer("popup", 100);
        const UiHandle popup = createHitRect(ui, ui.getLayerRoot(popupLayer), rectLayout(0.75f, 0.75f, 0.2f, 0.2f));

        UiModalDesc desc = modalDesc(popupLayer, popup, popup);
        desc.layerBlocking = UiModalLayerBlocking::None;
        desc.consumePointer = false;
        desc.consumeKeyboard = false;
        desc.consumeNavigation = false;
        desc.consumeTextInput = false;
        ui.pushModal(desc);

        const UiDispatchResult result = ui.dispatchInput(pointerFrame(0.1f, 0.1f), 1);
        require(result.modalActive, "non-blocking popup did not report modal state");
        require(!result.pointerBlocked, "non-blocking popup blocked pointer");
        require(result.hovered == base, "non-blocking popup prevented base hover");
    }
}

int main()
{
    testSingleModalPublishesState();
    testNestedModalsAndStackIntegrity();
    testFocusRestoration();
    testPointerBlockingAndLayerBlocking();
    testBlockingFlags();
    testCancelDismissesTopModalOnly();
    testManualDismiss();
    testHiddenAndDisabledLayerCleanup();
    testDestroyedModalCleanup();
    testHiddenAndDisabledOwnerCleanup();
    testNonBlockingPopupPolicy();
    return 0;
}
