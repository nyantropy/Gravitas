#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "UiSystem.h"
#include "VNDialogueComposition.h"
#include "VNPresentationProfile.h"
#include "VNRuntime.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "VN frontend runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    bool near(float lhs, float rhs, float epsilon = 0.001f)
    {
        return std::fabs(lhs - rhs) <= epsilon;
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

    UiInputFrame pointerFrame(float x, float y)
    {
        UiInputFrame frame;
        frame.pointerX = x;
        frame.pointerY = y;
        return frame;
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

    const UiRect& bounds(UiSystem& ui, UiHandle handle)
    {
        const UiNode* node = ui.findNode(handle);
        require(node != nullptr, "node missing");
        return node->computedLayout.bounds;
    }

    void extract(UiSystem& ui)
    {
        ui.extractCommandsRef(1280, 720);
    }

    gts::vn::VNDialogueComposition* mountVN(UiSystem& ui,
                                            gts::vn::VNRuntime& runtime,
                                            const gts::vn::VNDialogueUiConfig& config,
                                            UiCompositionId& outCompositionId)
    {
        runtime.presentExternalDialogue(
            "Guide",
            "Choose a path.",
            {
                {"Take the stairs", "stairs"},
                {"Open inventory", "inventory"},
                {"Ask merchant", "merchant"}
            });

        outCompositionId = ui.mountComposition(
            std::make_unique<gts::vn::VNDialogueComposition>(config, runtime));
        require(outCompositionId != UI_INVALID_COMPOSITION, "VN composition mount failed");
        require(ui.updateComposition(outCompositionId), "VN composition update failed");

        auto* composition =
            dynamic_cast<gts::vn::VNDialogueComposition*>(ui.findComposition(outCompositionId));
        require(composition != nullptr, "VN composition lookup failed");
        return composition;
    }

    std::vector<UiHandle> choiceOrder(UiSystem& ui)
    {
        return ui.navigationGraph().navigationOrder(
            ui.getDocument(),
            ui.focusManager(),
            ui.modalManager());
    }

    void testChoiceStackThemeAndNavigation()
    {
        UiSystem ui(nullptr);
        gts::vn::VNRuntime runtime;
        gts::vn::VNDialogueUiConfig config;
        config.profile.layout.maxChoices = 3;
        config.profile.layout.choices = {0.535f, 0.360f, 0.420f, 0.330f};

        UiCompositionId compositionId = UI_INVALID_COMPOSITION;
        gts::vn::VNDialogueComposition* composition = mountVN(ui, runtime, config, compositionId);
        extract(ui);

        UiComputedStyle choiceStyle;
        require(ui.theme().resolveStyle(gts::vn::VNThemeClass::ChoiceButton,
                                        UiStyleState::Hover,
                                        choiceStyle),
                "VN choice button style did not resolve from theme");
        require(choiceStyle.hasSkin, "VN choice style is not skin-driven");

        const std::vector<UiHandle> choices = choiceOrder(ui);
        require(choices.size() == 3, "VN choices were not registered as navigation nodes");

        const float expectedHeight = ui.theme().metricOr(gts::vn::VNThemeMetric::ChoiceButtonHeight, 0.0f);
        const float expectedGap = ui.theme().metricOr(gts::vn::VNThemeMetric::ChoiceGap, 0.0f);
        require(near(bounds(ui, choices[0]).height, expectedHeight), "choice height did not come from theme metric");
        require(near(bounds(ui, choices[1]).y - (bounds(ui, choices[0]).y + bounds(ui, choices[0]).height),
                     expectedGap),
                "choice gap did not come from stack metric");

        const UiDispatchResult& focusResult = ui.dispatchInput(navFrame(UiNavigationDirection::Next), 1);
        require(focusResult.navigationMoved, "navigation graph did not focus first VN choice");
        require(focusResult.navigationTo == choices[0], "VN navigation did not target first choice");

        const UiDispatchResult& submitResult = ui.dispatchInput(submitFrame(), 2);
        require(submitResult.navigationSubmitted, "VN choice did not receive navigation submit");
        require(composition->consumeClickedChoiceIndex() == 0, "VN composition did not expose semantic choice activation");
    }

    void testModalSlotOwnership()
    {
        UiSystem ui(nullptr);
        gts::vn::VNRuntime runtime;
        gts::vn::VNDialogueUiConfig config;
        UiCompositionId compositionId = UI_INVALID_COMPOSITION;
        mountVN(ui, runtime, config, compositionId);

        const UiHandle base = ui.createNode(UiNodeType::Rect, ui.getRoot());
        ui.setLayout(base, fillLayout());
        ui.setState(base, UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        ui.setPayload(base, UiRectData{{1.0f, 1.0f, 1.0f, 1.0f}});

        const UiLayerId modalLayer = ui.createLayer("vn-modal-slot", 400);
        require(modalLayer != UI_INVALID_LAYER, "modal layer creation failed");

        UiMountDesc modalSlotDesc;
        modalSlotDesc.name = "vn-modal-slot";
        modalSlotDesc.attachment.layer = modalLayer;
        const UiMountId modalSlot = ui.createMount(modalSlotDesc);
        require(modalSlot != UI_INVALID_MOUNT, "modal slot mount failed");

        UiMountDesc merchantDesc;
        merchantDesc.name = "merchant-modal";
        merchantDesc.attachment.parentMount = modalSlot;
        const UiMountId merchantMount = ui.createMount(merchantDesc);
        require(merchantMount != UI_INVALID_MOUNT, "merchant child mount failed");

        const UiHandle merchantRoot = ui.mountRoot(merchantMount);
        require(merchantRoot != UI_INVALID_HANDLE, "merchant mount root missing");
        ui.setLayout(merchantRoot, fillLayout());
        ui.setState(merchantRoot, UiStateFlags{.visible = true, .enabled = true, .interactable = false});

        UiModalDesc modal;
        modal.layer = modalLayer;
        modal.owner = merchantRoot;
        modal.ownerMount = merchantMount;
        modal.initialFocus = merchantRoot;
        const UiModalId modalId = ui.pushModal(modal);
        require(modalId != UI_INVALID_MODAL, "VN modal slot did not accept merchant modal");

        const UiDispatchResult& result = ui.dispatchInput(pointerFrame(0.05f, 0.05f), 1);
        require(result.modalActive, "merchant modal did not publish modal state");
        require(result.modalLayer == modalLayer, "merchant modal was not owned by VN modal layer");
        require(result.modalMount == merchantMount, "merchant modal was not owned by child mount");
        require(result.pointerBlocked, "merchant modal did not block lower VN/base interaction");
        require(result.hovered != base, "base UI received hover under merchant modal");
    }
} // namespace

int main()
{
    testChoiceStackThemeAndNavigation();
    testModalSlotOwnership();
    return 0;
}
