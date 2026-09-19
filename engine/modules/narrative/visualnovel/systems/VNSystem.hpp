#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "DialogueContextHelpers.h"
#include "DialogueRuntimeComponent.h"
#include "EcsExecutionProfile.h"
#include "InputBindingRegistry.h"
#include "RenderPassVisibilityComponent.h"
#include "UiSystem.h"
#include "InteractionFrontendSessionComponent.h"
#include "VNDialogueComposition.h"
#include "VNDialogueUi.h"
#include "VNExternalPresentationComponent.h"
#include "VNFrontendStateComponent.h"
#include "VNPlaybackStateComponent.h"

namespace gts::vn
{
    struct VNSystemConfig
    {
        VNRuntimeConfig runtime;
        VNDialogueUiConfig ui;
    };

    class VNSystem : public ECSControllerSystem
    {
    public:
        explicit VNSystem(VNSystemConfig inConfig = {})
            : config(std::move(inConfig)), runtime(config.runtime)
        {
            runtime.setMotionProfile(config.ui.profile.motionProfile);
        }

        VNRuntime& getRuntime()
        {
            return runtime;
        }

        const VNRuntime& getRuntime() const
        {
            return runtime;
        }

        void start(const VNScript& script)
        {
            runtime.start(script);
        }

        void start(VNScript&& script)
        {
            runtime.start(std::move(script));
        }

        void update(const EcsControllerContext& ctx) override
        {
            buildUiIfNeeded(ctx.world, ctx.ui);
            writeFrontendState(ctx.world, ctx.ui);

            if (dialogueActive(ctx.world))
            {
                updateDialoguePresentation(ctx);
                return;
            }

            if (interactionSessionActive(ctx.world))
            {
                updateInteractionPresentation(ctx);
                return;
            }

            if (runtime.isExternalDialogueActive())
                runtime.stopExternalDialogue();
            resetRenderPassVisibilityIfNeeded(ctx.world);
            resetDialogueExecutionProfileIfNeeded(ctx.world);
            lastExternalPresentationRevision = 0;

            VNRuntimeInput input;
            input.dt = ctx.time == nullptr ? 0.0f : ctx.time->unscaledDeltaTime;
            input.continuePressed = continuePressed(ctx);

            if (ctx.ui != nullptr
                && uiBuilt
                && runtime.isActive()
                && runtime.getConfig().capturePointerInput)
            {
                if (VNDialogueComposition* composition = dialogueComposition(ctx.ui))
                    input.clickedChoiceIndex = composition->consumeClickedChoiceIndex();
            }

            runtime.update(ctx, input);
            writePlaybackState(ctx.world);

            syncUi(ctx.ui);
        }

    private:
        VNSystemConfig config;
        VNRuntime runtime;
        UiCompositionId dialogueCompositionId = UI_INVALID_COMPOSITION;
        UiLayerId interactionLayerId = UI_INVALID_LAYER;
        UiMountId interactionSlotMountId = UI_INVALID_MOUNT;
        UiLayerId modalLayerId = UI_INVALID_LAYER;
        UiMountId modalSlotMountId = UI_INVALID_MOUNT;
        bool uiBuilt = false;
        bool renderPassVisibilityOverridden = false;
        bool executionProfilePushed = false;
        std::string activeExecutionProfileId;
        uint64_t lastExternalPresentationRevision = 0;

        static bool dialogueActive(ECSWorld& world)
        {
            return world.hasAny<gts::dialogue::DialogueRuntimeComponent>()
                && world.getSingleton<gts::dialogue::DialogueRuntimeComponent>().runtime.isActive();
        }

        void buildUiIfNeeded(ECSWorld& world, UiSystem* ui)
        {
            if (uiBuilt || ui == nullptr)
                return;

            UiMountDesc desc;
            desc.name = "vn-dialogue";
            dialogueCompositionId = ui->mountComposition(
                std::make_unique<VNDialogueComposition>(config.ui, runtime),
                desc);
            if (dialogueCompositionId == UI_INVALID_COMPOSITION)
                return;

            const UiSurfaceId surface = ui->compositionSurface(dialogueCompositionId);
            const VNInteractionLayout layout = resolveVNInteractionLayout(config.ui.profile);

            interactionLayerId = ui->createLayer(surface, "interaction-frontend-view", 300);
            if (interactionLayerId != UI_INVALID_LAYER)
            {
                UiMountDesc interactionSlotDesc;
                interactionSlotDesc.name = "interaction-slot";
                interactionSlotDesc.attachment.layer = interactionLayerId;
                interactionSlotMountId = ui->createMount(surface, interactionSlotDesc);

                const UiHandle interactionRoot = ui->mountRoot(surface, interactionSlotMountId);
                if (interactionRoot != UI_INVALID_HANDLE)
                {
                    ui->setLayout(surface, interactionRoot, slotLayout(layout.interactionSlot));
                    ui->setStyleClass(surface, interactionRoot, VNThemeClass::InteractionSlot);
                    ui->setState(surface,
                                 interactionRoot,
                                 UiStateFlags{.visible = true, .enabled = true, .interactable = false});
                }
            }

            modalLayerId = ui->createLayer(surface, "interaction-modal-slot", 400);
            if (modalLayerId != UI_INVALID_LAYER)
            {
                UiMountDesc modalSlotDesc;
                modalSlotDesc.name = "interaction-modal-slot";
                modalSlotDesc.attachment.layer = modalLayerId;
                modalSlotMountId = ui->createMount(surface, modalSlotDesc);
            }

            uiBuilt = true;
            writeFrontendState(world, ui);
        }

        VNDialogueComposition* dialogueComposition(UiSystem* ui) const
        {
            if (ui == nullptr || dialogueCompositionId == UI_INVALID_COMPOSITION)
                return nullptr;

            return dynamic_cast<VNDialogueComposition*>(ui->findComposition(dialogueCompositionId));
        }

        static UiLayoutSpec slotLayout(const VNOverlaySlotLayout& slot)
        {
            const UiRect rect = vnCompatibilityRectFromOverlaySlot(slot);
            UiLayoutSpec layout;
            layout.positionMode = UiPositionMode::Anchored;
            layout.widthMode = UiSizeMode::FromAnchors;
            layout.heightMode = UiSizeMode::FromAnchors;
            layout.anchorMin = {std::clamp(rect.x, 0.0f, 1.0f), std::clamp(rect.y, 0.0f, 1.0f)};
            layout.anchorMax = {
                std::clamp(rect.x + rect.width, 0.0f, 1.0f),
                std::clamp(rect.y + rect.height, 0.0f, 1.0f)
            };
            return layout;
        }

        void syncUi(UiSystem* ui)
        {
            if (ui != nullptr && uiBuilt)
                ui->updateComposition(dialogueCompositionId);
        }

        void writeFrontendState(ECSWorld& world, UiSystem* ui) const
        {
            if (!world.hasAny<VNFrontendStateComponent>())
                world.createSingleton<VNFrontendStateComponent>();

            VNFrontendStateComponent& state = world.getSingleton<VNFrontendStateComponent>();
            state.mounted = uiBuilt && ui != nullptr;
            state.surface = ui == nullptr ? UI_DEFAULT_SURFACE : ui->compositionSurface(dialogueCompositionId);
            state.composition = dialogueCompositionId;
            state.interactionLayer = interactionLayerId;
            state.interactionSlotMount = interactionSlotMountId;
            state.interactionSlotRoot = ui == nullptr || interactionSlotMountId == UI_INVALID_MOUNT
                ? UI_INVALID_HANDLE
                : ui->mountRoot(state.surface, interactionSlotMountId);
            state.interactionSlot =
                vnCompatibilityRectFromOverlaySlot(resolveVNInteractionLayout(config.ui.profile).interactionSlot);
            state.modalLayer = modalLayerId;
            state.modalSlotMount = modalSlotMountId;
            state.modalSlotRoot = ui == nullptr || modalSlotMountId == UI_INVALID_MOUNT
                ? UI_INVALID_HANDLE
                : ui->mountRoot(state.surface, modalSlotMountId);
        }

        bool continuePressed(const EcsControllerContext& ctx) const
        {
            const std::string& action = runtime.getConfig().continueAction;
            if (ctx.input == nullptr || action.empty())
                return false;

            if (ctx.input->isPressed(action))
                return true;

            const std::optional<InputTrigger> lastPressed = ctx.input->getLastPressedTrigger();
            if (!lastPressed.has_value())
                return false;

            const std::vector<InputTrigger> triggers = ctx.input->getTriggersForAction(action);
            return std::find(triggers.begin(), triggers.end(), *lastPressed) != triggers.end();
        }

        void updateDialoguePresentation(const EcsControllerContext& ctx)
        {
            auto& dialogueRuntime =
                ctx.world.getSingleton<gts::dialogue::DialogueRuntimeComponent>().runtime;
            gts::dialogue::DialogueContext dialogueContext =
                gts::dialogue::makeDialogueContext(ctx, dialogueRuntime);

            const bool acceptDialogueInput = runtime.isExternalDialogueActive();
            const bool continueInput = acceptDialogueInput && continuePressed(ctx);
            int clickedChoiceIndex = -1;
            if (acceptDialogueInput
                && ctx.ui != nullptr
                && uiBuilt
                && runtime.getConfig().capturePointerInput)
            {
                if (VNDialogueComposition* composition = dialogueComposition(ctx.ui))
                    clickedChoiceIndex = composition->consumeClickedChoiceIndex();
            }

            if (clickedChoiceIndex >= 0)
                dialogueRuntime.selectChoice(static_cast<size_t>(clickedChoiceIndex), dialogueContext);
            else if (continueInput && dialogueRuntime.getVisibleChoices().empty())
                dialogueRuntime.advance(dialogueContext);

            VNRuntimeInput input;
            input.dt = ctx.time == nullptr ? 0.0f : ctx.time->unscaledDeltaTime;
            input.continuePressed = false;
            input.clickedChoiceIndex = -1;

            if (dialogueRuntime.isActive())
            {
                dialogueRuntime.refreshVisibleChoices(dialogueContext);
                presentDialogueRuntime(dialogueRuntime);
                const bool fullscreenPresentation = applyExternalPresentation(ctx.world);
                applyDialogueExecutionProfile(ctx.world, fullscreenPresentation);
                runtime.update(ctx, input);
            }
            else
            {
                if (interactionSessionActive(ctx.world))
                {
                    updateInteractionPresentation(ctx);
                    return;
                }

                runtime.stopExternalDialogue();
                resetRenderPassVisibilityIfNeeded(ctx.world);
                resetDialogueExecutionProfileIfNeeded(ctx.world);
                lastExternalPresentationRevision = 0;
            }

            writePlaybackState(ctx.world);
            syncUi(ctx.ui);
        }

        static bool interactionSessionActive(ECSWorld& world)
        {
            return world.hasAny<InteractionFrontendSessionComponent>()
                && world.getSingleton<InteractionFrontendSessionComponent>().isInteractionViewActive();
        }

        void updateInteractionPresentation(const EcsControllerContext& ctx)
        {
            const InteractionFrontendSessionComponent& session =
                ctx.world.getSingleton<InteractionFrontendSessionComponent>();

            const bool fullscreenPresentation = applyExternalPresentation(ctx.world);
            applyDialogueExecutionProfile(ctx.world, fullscreenPresentation);

            if (session.feedbackText.empty())
                runtime.presentExternalStage();
            else
                runtime.presentExternalDialogue(session.speakerName, session.feedbackText, {});

            VNRuntimeInput input;
            input.dt = ctx.time == nullptr ? 0.0f : ctx.time->unscaledDeltaTime;
            input.continuePressed = false;
            input.clickedChoiceIndex = -1;
            runtime.update(ctx, input);

            writePlaybackState(ctx.world);
            syncUi(ctx.ui);
        }

        void presentDialogueRuntime(const gts::dialogue::DialogueRuntime& dialogueRuntime)
        {
            const gts::dialogue::DialogueNode* node = dialogueRuntime.getCurrentNode();
            if (node == nullptr)
                return;

            std::vector<VNChoiceOption> choices;
            choices.reserve(dialogueRuntime.getVisibleChoices().size());
            for (const gts::dialogue::DialogueVisibleChoice& choice : dialogueRuntime.getVisibleChoices())
            {
                choices.push_back(VNChoiceOption{
                    choice.text,
                    choice.targetNode
                });
            }

            runtime.presentExternalDialogue(node->speaker, node->text, choices);
        }

        bool applyExternalPresentation(ECSWorld& world)
        {
            VNStage& stage = runtime.getStage();

            if (!world.hasAny<VNExternalPresentationComponent>())
            {
                stage.clearSprites();
                stage.clearBackgroundOverride();
                stage.setDimming(0.0f);
                resetRenderPassVisibilityIfNeeded(world);
                lastExternalPresentationRevision = 0;
                return false;
            }

            VNExternalPresentationComponent& presentation =
                world.getSingleton<VNExternalPresentationComponent>();
            if (!presentation.active)
            {
                stage.clearSprites();
                stage.clearBackgroundOverride();
                stage.setDimming(0.0f);
                resetRenderPassVisibilityIfNeeded(world);
                lastExternalPresentationRevision = 0;
                return false;
            }

            if (presentation.revision != lastExternalPresentationRevision
                || !presentation.animationRequests.empty())
            {
                stage.setBackground(presentation.background);
                stage.setDimming(presentation.dimmingAlpha);
                stage.clearSprites();
                for (const VNExternalSprite& sprite : presentation.sprites)
                {
                    stage.showSprite(sprite.id,
                                     sprite.sprite,
                                     sprite.appearDurationSeconds,
                                     sprite.appearEase);
                }

                for (const VNExternalSpriteAnimation& animation : presentation.animationRequests)
                {
                    stage.shakeSprite(animation.spriteId,
                                      animation.durationSeconds,
                                      animation.intensity,
                                      animation.frequency,
                                      animation.ease);
                }
                presentation.animationRequests.clear();
                lastExternalPresentationRevision = presentation.revision;
            }

            writeRenderPassVisibility(world, presentation);
            return isFullscreenPresentation(presentation);
        }

        static bool isFullscreenPresentation(const VNExternalPresentationComponent& presentation)
        {
            return presentation.suppressSceneRendering
                || presentation.background.mode == VNBackgroundMode::FullscreenImage
                || presentation.background.mode == VNBackgroundMode::SolidColor;
        }

        void writeRenderPassVisibility(ECSWorld& world,
                                       const VNExternalPresentationComponent& presentation)
        {
            if (!world.hasAny<RenderPassVisibilityComponent>())
                world.createSingleton<RenderPassVisibilityComponent>();

            RenderPassVisibilityComponent& visibility =
                world.getSingleton<RenderPassVisibilityComponent>();
            visibility.renderScene = !presentation.suppressSceneRendering;
            visibility.renderParticles = !presentation.suppressParticles;
            renderPassVisibilityOverridden = true;
        }

        void resetRenderPassVisibilityIfNeeded(ECSWorld& world)
        {
            if (!renderPassVisibilityOverridden)
                return;

            if (world.hasAny<RenderPassVisibilityComponent>())
                world.getSingleton<RenderPassVisibilityComponent>() =
                    RenderPassVisibilityComponent::allVisible();

            renderPassVisibilityOverridden = false;
        }

        void applyDialogueExecutionProfile(ECSWorld& world, bool fullscreenPresentation)
        {
            SceneExecutionProfile profile = fullscreenPresentation
                ? SceneExecutionProfile::fullscreenDialogue()
                : SceneExecutionProfile::dialogueOverlay();
            const std::string profileId = profile.id;

            if (executionProfilePushed && activeExecutionProfileId == profileId)
                return;

            resetDialogueExecutionProfileIfNeeded(world);
            if (executionProfilePushed)
                return;

            world.pushExecutionProfile(std::move(profile));
            activeExecutionProfileId = profileId;
            executionProfilePushed = true;
        }

        void resetDialogueExecutionProfileIfNeeded(ECSWorld& world)
        {
            if (!executionProfilePushed)
                return;

            if (!world.popExecutionProfile(activeExecutionProfileId))
                return;

            activeExecutionProfileId.clear();
            executionProfilePushed = false;
        }

        void writePlaybackState(ECSWorld& world) const
        {
            if (!world.hasAny<VNPlaybackStateComponent>())
                world.createSingleton<VNPlaybackStateComponent>();

            VNPlaybackStateComponent& state = world.getSingleton<VNPlaybackStateComponent>();
            state.active = runtime.isActive();
            state.blockingGameplayInput = runtime.isBlockingGameplayInput();
            state.waitingForInput = runtime.getStatus() == VNRuntimeStatus::WaitingForInput;
            state.waitingForChoice = runtime.getStatus() == VNRuntimeStatus::WaitingForChoice;
            state.waitingForAnimation = runtime.getStatus() == VNRuntimeStatus::WaitingForAnimation;
            state.status = runtime.getStatus();
        }
    };
} // namespace gts::vn
