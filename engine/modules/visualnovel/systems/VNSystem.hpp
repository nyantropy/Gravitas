#pragma once

#include <algorithm>
#include <string>
#include <utility>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "InputBindingRegistry.h"
#include "UiSystem.h"
#include "VNDialogueUi.h"
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
