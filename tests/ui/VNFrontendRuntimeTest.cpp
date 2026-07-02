#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "InteractionFrontendSessionComponent.h"
#include "UiSystem.h"
#include "VNExternalPresentationComponent.h"
#include "VNDialogueComposition.h"
#include "VNFrontendStateComponent.h"
#include "VNPlaybackStateComponent.h"
#include "VNPresentationProfile.h"
#include "VNRuntime.h"
#include "VNSystem.hpp"

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

    UiLayoutSpec anchoredRect(float x, float y, float width, float height)
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Anchored;
        layout.widthMode = UiSizeMode::FromAnchors;
        layout.heightMode = UiSizeMode::FromAnchors;
        layout.anchorMin = {x, y};
        layout.anchorMax = {x + width, y + height};
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

    void testInteractionSlotPublishedByVNSystem()
    {
        ECSWorld world;
        UiSystem ui(nullptr);

        gts::vn::VNSystemConfig config;
        config.ui.profile.layout.interaction = {0.125f, 0.150f, 0.750f, 0.450f};
        gts::vn::VNSystem system(config);

        EcsControllerContext ctx{world};
        ctx.ui = &ui;
        ctx.windowPixelWidth = 1280.0f;
        ctx.windowPixelHeight = 720.0f;

        system.update(ctx);
        extract(ui);

        require(world.hasAny<gts::vn::VNFrontendStateComponent>(), "VN frontend state was not published");
        const auto& frontend = world.getSingleton<gts::vn::VNFrontendStateComponent>();
        require(frontend.mounted, "VN frontend state did not mark the frontend mounted");
        require(frontend.interactionLayer != UI_INVALID_LAYER, "interaction layer was not created");
        require(frontend.interactionSlotMount != UI_INVALID_MOUNT, "interaction slot mount was not created");
        require(frontend.interactionSlotRoot != UI_INVALID_HANDLE, "interaction slot root was not published");
        require(frontend.modalSlotMount != UI_INVALID_MOUNT, "modal slot remained unavailable");

        const UiRect& slotBounds = bounds(ui, frontend.interactionSlotRoot);
        require(near(slotBounds.x, config.ui.profile.layout.interaction.x), "interaction slot x did not use profile");
        require(near(slotBounds.y, config.ui.profile.layout.interaction.y), "interaction slot y did not use profile");
        require(near(slotBounds.width, config.ui.profile.layout.interaction.width),
                "interaction slot width did not use profile");
        require(near(slotBounds.height, config.ui.profile.layout.interaction.height),
                "interaction slot height did not use profile");

        const UiHandle base = ui.createNode(UiNodeType::Rect, ui.getRoot());
        ui.setLayout(base, fillLayout());
        ui.setState(base, UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        ui.setPayload(base, UiRectData{{1.0f, 1.0f, 1.0f, 1.0f}});

        UiMountDesc merchantDesc;
        merchantDesc.name = "merchant-trade-view";
        merchantDesc.attachment.parentMount = frontend.interactionSlotMount;
        const UiMountId merchantMount = ui.createMount(frontend.surface, merchantDesc);
        require(merchantMount != UI_INVALID_MOUNT, "merchant child mount failed under interaction slot");

        const UiHandle merchantRoot = ui.mountRoot(frontend.surface, merchantMount);
        require(merchantRoot != UI_INVALID_HANDLE, "merchant child mount root missing");
        ui.setLayout(frontend.surface, merchantRoot, fillLayout());

        const UiHandle merchantPanel = ui.createNode(frontend.surface, UiNodeType::Rect, merchantRoot);
        ui.setLayout(frontend.surface, merchantPanel, fillLayout());
        ui.setState(frontend.surface,
                    merchantPanel,
                    UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        ui.setPayload(frontend.surface, merchantPanel, UiRectData{{0.0f, 0.0f, 0.0f, 1.0f}});
        extract(ui);

        const UiDispatchResult& result = ui.dispatchInput(
            pointerFrame(frontend.interactionSlot.x + frontend.interactionSlot.width * 0.5f,
                         frontend.interactionSlot.y + frontend.interactionSlot.height * 0.5f),
            1);
        require(result.hovered == merchantPanel, "interaction slot child did not own input above default root");

        const UiHandle merchantButton = ui.createNode(frontend.surface, UiNodeType::Rect, merchantRoot);
        ui.setLayout(frontend.surface, merchantButton, anchoredRect(0.62f, 0.70f, 0.18f, 0.12f));
        ui.setState(frontend.surface,
                    merchantButton,
                    UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        ui.setPayload(frontend.surface, merchantButton, UiRectData{{0.2f, 0.4f, 0.8f, 1.0f}});
        extract(ui);

        const UiRect& buttonBounds = bounds(ui, merchantButton);
        require(near(buttonBounds.x, slotBounds.x + slotBounds.width * 0.62f),
                "interaction slot child x was not parent-relative");
        require(near(buttonBounds.y, slotBounds.y + slotBounds.height * 0.70f),
                "interaction slot child y was not parent-relative");
        require(near(buttonBounds.width, slotBounds.width * 0.18f),
                "interaction slot child width was not parent-relative");
        require(near(buttonBounds.height, slotBounds.height * 0.12f),
                "interaction slot child height was not parent-relative");

        const UiDispatchResult& buttonResult = ui.dispatchInput(
            pointerFrame(buttonBounds.x + buttonBounds.width * 0.5f,
                         buttonBounds.y + buttonBounds.height * 0.5f),
            2);
        require(buttonResult.hovered == merchantButton,
                "interaction slot child hitbox did not match parent-relative visual bounds");
    }

    void testInteractionSessionKeepsStageActive()
    {
        ECSWorld world;
        UiSystem ui(nullptr);

        gts::vn::VNExternalPresentationComponent presentation;
        presentation.active = true;
        presentation.background.mode = gts::vn::VNBackgroundMode::SolidColor;
        presentation.background.color = {0.12f, 0.10f, 0.08f, 1.0f};
        world.createSingleton<gts::vn::VNExternalPresentationComponent>(presentation);

        gts::vn::InteractionFrontendSessionComponent session;
        session.active = true;
        session.mode = gts::vn::InteractionFrontendMode::MerchantTrade;
        session.npcId = "merchant";
        session.npcName = "Merchant";
        session.merchantId = "merchant";
        session.speakerName = "Merchant";
        session.feedbackText = "Pleasure doing business.";
        world.createSingleton<gts::vn::InteractionFrontendSessionComponent>(session);

        gts::vn::VNSystem system;
        EcsControllerContext ctx{world};
        ctx.ui = &ui;
        ctx.windowPixelWidth = 1280.0f;
        ctx.windowPixelHeight = 720.0f;

        system.update(ctx);
        extract(ui);

        require(world.hasAny<gts::vn::VNPlaybackStateComponent>(), "playback state was not written");
        const auto& playback = world.getSingleton<gts::vn::VNPlaybackStateComponent>();
        require(playback.active, "interaction session did not keep VN runtime active");
        require(system.getRuntime().isExternalDialogueActive(), "interaction session did not own external VN mode");
        require(system.getRuntime().getDialogue().visible, "merchant feedback did not use dialogue panel");
        require(system.getRuntime().getDialogue().speaker == "Merchant", "merchant speaker did not reach VN dialogue");

        auto& activeSession = world.getSingleton<gts::vn::InteractionFrontendSessionComponent>();
        activeSession.feedbackText.clear();
        ++activeSession.revision;
        system.update(ctx);
        require(system.getRuntime().isActive(), "stage-only interaction session stopped VN runtime");
        require(!system.getRuntime().getDialogue().visible, "stage-only interaction session left dialogue visible");
    }
} // namespace

int main()
{
    testChoiceStackThemeAndNavigation();
    testModalSlotOwnership();
    testInteractionSlotPublishedByVNSystem();
    testInteractionSessionKeepsStageActive();
    return 0;
}
