#pragma once

#include <algorithm>
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
#include "VNDialogueUi.h"
#include "VNExternalPresentationComponent.h"
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
            buildUiIfNeeded(ctx.ui);

            if (dialogueActive(ctx.world))
            {
                updateDialoguePresentation(ctx);
                return;
            }

            if (runtime.isExternalDialogueActive())
                runtime.stopExternalDialogue();
            resetRenderPassVisibilityIfNeeded(ctx.world);
            resetDialogueExecutionProfileIfNeeded(ctx.world);

            VNRuntimeInput input;
            input.dt = ctx.time == nullptr ? 0.0f : ctx.time->unscaledDeltaTime;
            input.continuePressed = continuePressed(ctx);

            if (ctx.ui != nullptr
                && uiBuilt
                && runtime.isActive()
                && runtime.getConfig().capturePointerInput)
            {
                const UiInteractionResult interaction = updateInteraction(ctx);
                input.clickedChoiceIndex = VNDialogueUi::choiceIndexFromInteraction(handles, interaction);
            }

            runtime.update(ctx, input);
            writePlaybackState(ctx.world);

            if (ctx.ui != nullptr && uiBuilt)
                VNDialogueUi::sync(*ctx.ui, handles, config.ui, runtime);
        }

    private:
        VNSystemConfig config;
        VNRuntime runtime;
        VNDialogueUiHandles handles;
        bool uiBuilt = false;
        bool renderPassVisibilityOverridden = false;
        bool executionProfilePushed = false;
        std::string activeExecutionProfileId;

        static bool dialogueActive(ECSWorld& world)
        {
            return world.hasAny<gts::dialogue::DialogueRuntimeComponent>()
                && world.getSingleton<gts::dialogue::DialogueRuntimeComponent>().runtime.isActive();
        }

        void buildUiIfNeeded(UiSystem* ui)
        {
            if (uiBuilt || ui == nullptr)
                return;

            handles = VNDialogueUi::build(*ui, config.ui);
            uiBuilt = true;
        }

        bool continuePressed(const EcsControllerContext& ctx) const
        {
            const std::string& action = runtime.getConfig().continueAction;
            return ctx.input != nullptr && !action.empty() && ctx.input->isPressed(action);
        }

        UiInteractionResult updateInteraction(const EcsControllerContext& ctx) const
        {
            if (ctx.input == nullptr || ctx.ui == nullptr)
                return {};

            const float width = std::max(1.0f, ctx.windowPixelWidth);
            const float height = std::max(1.0f, ctx.windowPixelHeight);

            UiInputFrame frame;
            frame.pointerX = std::clamp(static_cast<float>(ctx.input->mouseX()) / width, 0.0f, 1.0f);
            frame.pointerY = std::clamp(static_cast<float>(ctx.input->mouseY()) / height, 0.0f, 1.0f);
            frame.primaryDown = ctx.input->isHeld("engine.ui_primary");
            frame.primaryPressed = ctx.input->isPressed("engine.ui_primary");
            frame.primaryReleased = ctx.input->isReleased("engine.ui_primary");
            frame.scrollX = static_cast<float>(ctx.input->scrollX());
            frame.scrollY = static_cast<float>(ctx.input->scrollY());
            return ctx.ui->updateInteraction(frame);
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
                const UiInteractionResult interaction = updateInteraction(ctx);
                clickedChoiceIndex = VNDialogueUi::choiceIndexFromInteraction(handles, interaction);
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
                runtime.stopExternalDialogue();
                resetRenderPassVisibilityIfNeeded(ctx.world);
                resetDialogueExecutionProfileIfNeeded(ctx.world);
            }

            writePlaybackState(ctx.world);
            if (ctx.ui != nullptr && uiBuilt)
                VNDialogueUi::sync(*ctx.ui, handles, config.ui, runtime);
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
            stage.clearSprites();

            if (!world.hasAny<VNExternalPresentationComponent>())
            {
                stage.clearBackgroundOverride();
                stage.setDimming(0.0f);
                resetRenderPassVisibilityIfNeeded(world);
                return false;
            }

            const VNExternalPresentationComponent& presentation =
                world.getSingleton<VNExternalPresentationComponent>();
            if (!presentation.active)
            {
                stage.clearBackgroundOverride();
                stage.setDimming(0.0f);
                resetRenderPassVisibilityIfNeeded(world);
                return false;
            }

            stage.setBackground(presentation.background);
            stage.setDimming(presentation.dimmingAlpha);
            for (const VNExternalSprite& sprite : presentation.sprites)
                stage.showSprite(sprite.id, sprite.sprite);
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
