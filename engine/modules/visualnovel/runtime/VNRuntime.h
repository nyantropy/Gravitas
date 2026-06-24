#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "VNCommandRegistry.h"
#include "VNStage.h"
#include "VNTypes.h"

namespace gts::vn
{
    enum class VNRuntimeStatus
    {
        Idle = 0,
        Running,
        WaitingForInput,
        WaitingForAnimation,
        WaitingForTimer,
        WaitingForChoice,
        WaitingForExternalCommand,
        Complete
    };

    struct VNRuntimeInput
    {
        float dt = 0.0f;
        bool continuePressed = false;
        int clickedChoiceIndex = -1;
    };

    class VNRuntime
    {
    public:
        explicit VNRuntime(VNRuntimeConfig inConfig = {})
            : config(std::move(inConfig))
        {
        }

        void setConfig(const VNRuntimeConfig& inConfig)
        {
            config = inConfig;
            dialogue.typewriterCharactersPerSecond = config.typewriterCharactersPerSecond;
        }

        void setMotionProfile(const VNSpriteMotionProfile& inMotionProfile)
        {
            motionProfile = inMotionProfile;
        }

        const VNRuntimeConfig& getConfig() const
        {
            return config;
        }

        void start(const VNScript& inScript)
        {
            script = inScript;
            cursor = 0;
            waitRemainingSeconds = 0.0f;
            status = VNRuntimeStatus::Running;
            dialogue.clear();
            dialogue.typewriterCharactersPerSecond = config.typewriterCharactersPerSecond;
        }

        void start(VNScript&& inScript)
        {
            script = std::move(inScript);
            cursor = 0;
            waitRemainingSeconds = 0.0f;
            status = VNRuntimeStatus::Running;
            dialogue.clear();
            dialogue.typewriterCharactersPerSecond = config.typewriterCharactersPerSecond;
        }

        void stop()
        {
            status = VNRuntimeStatus::Idle;
            externalDialogueActive = false;
            cursor = 0;
            waitRemainingSeconds = 0.0f;
            dialogue.clear();
            stage.clear();
        }

        void resumeExternalCommand()
        {
            if (status == VNRuntimeStatus::WaitingForExternalCommand)
                status = VNRuntimeStatus::Running;
        }

        void presentExternalDialogue(const std::string& speaker,
                                     const std::string& text,
                                     const std::vector<VNChoiceOption>& choices)
        {
            externalDialogueActive = true;
            const bool textChanged = dialogue.speaker != speaker || dialogue.text != text;
            dialogue.visible = true;
            dialogue.speaker = speaker;
            dialogue.text = text;
            dialogue.choices = choices;
            dialogue.waitingForContinue = choices.empty();
            dialogue.continueIndicatorVisible = choices.empty()
                && dialogue.visibleCharacters >= dialogue.text.size();

            if (textChanged)
            {
                dialogue.visibleText.clear();
                dialogue.visibleCharacters = 0;
                dialogue.typewriterAccumulator = 0.0f;
                dialogue.typewriterCharactersPerSecond = config.typewriterCharactersPerSecond;
            }

            status = choices.empty()
                ? VNRuntimeStatus::WaitingForInput
                : VNRuntimeStatus::WaitingForChoice;
        }

        void stopExternalDialogue()
        {
            if (externalDialogueActive)
            {
                externalDialogueActive = false;
                stop();
            }
        }

        bool isExternalDialogueActive() const
        {
            return externalDialogueActive;
        }

        void update(const EcsControllerContext& ctx, const VNRuntimeInput& input)
        {
            if (status == VNRuntimeStatus::Idle || status == VNRuntimeStatus::Complete)
                return;

            const float dt = std::max(0.0f, input.dt);
            stage.update(dt);
            updateTypewriter(dt);
            updateWaitState(input);

            if (status == VNRuntimeStatus::Running)
                processCommands(ctx);
        }

        bool jumpToLabel(const std::string& label)
        {
            size_t nextCursor = 0;
            if (!script.findLabel(label, nextCursor))
                return false;

            cursor = nextCursor;
            status = VNRuntimeStatus::Running;
            return true;
        }

        bool isActive() const
        {
            return status != VNRuntimeStatus::Idle && status != VNRuntimeStatus::Complete;
        }

        bool isBlockingGameplayInput() const
        {
            return isActive() && config.blockGameplayInput;
        }

        VNRuntimeStatus getStatus() const
        {
            return status;
        }

        VNStage& getStage()
        {
            return stage;
        }

        const VNStage& getStage() const
        {
            return stage;
        }

        VNDialogueState& getDialogue()
        {
            return dialogue;
        }

        const VNDialogueState& getDialogue() const
        {
            return dialogue;
        }

        VNCommandRegistry& getCommandRegistry()
        {
            return commandRegistry;
        }

        const VNCommandRegistry& getCommandRegistry() const
        {
            return commandRegistry;
        }

    private:
        VNRuntimeConfig config;
        VNScript script;
        VNStage stage;
        VNDialogueState dialogue;
        VNCommandRegistry commandRegistry;
        VNSpriteMotionProfile motionProfile;
        VNRuntimeStatus status = VNRuntimeStatus::Idle;
        size_t cursor = 0;
        float waitRemainingSeconds = 0.0f;
        bool externalDialogueActive = false;

        void updateWaitState(const VNRuntimeInput& input)
        {
            switch (status)
            {
                case VNRuntimeStatus::WaitingForTimer:
                    waitRemainingSeconds -= input.dt;
                    if (waitRemainingSeconds <= 0.0f)
                        status = VNRuntimeStatus::Running;
                    break;
                case VNRuntimeStatus::WaitingForAnimation:
                    if (!stage.hasActiveTweens())
                        status = VNRuntimeStatus::Running;
                    break;
                case VNRuntimeStatus::WaitingForInput:
                    if (input.continuePressed)
                        advanceDialogueInput();
                    break;
                case VNRuntimeStatus::WaitingForChoice:
                    if (input.clickedChoiceIndex >= 0)
                        choose(static_cast<size_t>(input.clickedChoiceIndex));
                    break;
                default:
                    break;
            }
        }

        void updateTypewriter(float dt)
        {
            if (!dialogue.visible || dialogue.text.empty())
                return;

            if (dialogue.visibleCharacters >= dialogue.text.size())
            {
                dialogue.visibleText = dialogue.text;
                dialogue.continueIndicatorVisible = status == VNRuntimeStatus::WaitingForInput;
                return;
            }

            if (dialogue.typewriterCharactersPerSecond <= 0.0f)
            {
                revealDialogueText();
                return;
            }

            dialogue.typewriterAccumulator += dt * dialogue.typewriterCharactersPerSecond;
            const size_t visible = std::min(dialogue.text.size(),
                                            static_cast<size_t>(dialogue.typewriterAccumulator));
            if (visible != dialogue.visibleCharacters)
            {
                dialogue.visibleCharacters = visible;
                dialogue.visibleText = dialogue.text.substr(0, dialogue.visibleCharacters);
            }

            dialogue.continueIndicatorVisible =
                status == VNRuntimeStatus::WaitingForInput
                && dialogue.visibleCharacters >= dialogue.text.size();
        }

        void revealDialogueText()
        {
            dialogue.visibleCharacters = dialogue.text.size();
            dialogue.visibleText = dialogue.text;
            dialogue.typewriterAccumulator = static_cast<float>(dialogue.visibleCharacters);
            dialogue.continueIndicatorVisible = status == VNRuntimeStatus::WaitingForInput;
        }

        void advanceDialogueInput()
        {
            if (dialogue.visibleCharacters < dialogue.text.size())
            {
                revealDialogueText();
                return;
            }

            dialogue.waitingForContinue = false;
            dialogue.continueIndicatorVisible = false;
            status = VNRuntimeStatus::Running;
        }

        void choose(size_t index)
        {
            if (index >= dialogue.choices.size())
                return;

            const std::string targetLabel = dialogue.choices[index].targetLabel;
            dialogue.choices.clear();
            dialogue.visible = false;
            dialogue.waitingForContinue = false;
            dialogue.continueIndicatorVisible = false;

            if (!targetLabel.empty())
                jumpToLabel(targetLabel);
            else
                status = VNRuntimeStatus::Running;
        }

        void processCommands(const EcsControllerContext& ctx)
        {
            constexpr size_t MaxCommandsPerFrame = 512;
            size_t processed = 0;

            while (status == VNRuntimeStatus::Running)
            {
                if (cursor >= script.commands.size())
                {
                    completeScript();
                    return;
                }

                if (++processed > MaxCommandsPerFrame)
                {
                    status = VNRuntimeStatus::WaitingForExternalCommand;
                    return;
                }

                VNCommand command = script.commands[cursor++];
                executeCommand(ctx, command);
            }
        }

        void executeCommand(const EcsControllerContext& ctx, const VNCommand& command)
        {
            switch (command.type)
            {
                case VNCommandType::Say:
                    executeSay(command);
                    break;
                case VNCommandType::ShowSprite:
                    executeShowSprite(command);
                    break;
                case VNCommandType::HideSprite:
                    stage.hideSprite(command.target, command.durationSeconds, command.ease);
                    waitForAnimationIfRequested(command);
                    break;
                case VNCommandType::MoveSprite:
                    stage.moveSprite(command.target, command.position, command.durationSeconds, command.ease);
                    waitForAnimationIfRequested(command);
                    break;
                case VNCommandType::FadeSprite:
                    stage.fadeSprite(command.target, command.alpha, command.durationSeconds, command.ease);
                    waitForAnimationIfRequested(command);
                    break;
                case VNCommandType::ScaleSprite:
                    stage.scaleSprite(command.target, command.scale, command.durationSeconds, command.ease);
                    waitForAnimationIfRequested(command);
                    break;
                case VNCommandType::RotateSprite:
                    stage.rotateSprite(command.target, command.rotation, command.durationSeconds, command.ease);
                    waitForAnimationIfRequested(command);
                    break;
                case VNCommandType::ShakeSprite:
                    stage.shakeSprite(command.target,
                                      command.durationSeconds,
                                      command.intensity,
                                      command.frequency,
                                      command.ease);
                    waitForAnimationIfRequested(command);
                    break;
                case VNCommandType::AnimateSprite:
                    executeAnimateSprite(command);
                    break;
                case VNCommandType::ChangeExpression:
                    stage.changeExpression(command.target, command.imageAsset, command.expression);
                    break;
                case VNCommandType::SetBackground:
                    executeSetBackground(command);
                    break;
                case VNCommandType::SetDimming:
                    stage.setDimming(command.alpha, command.durationSeconds, command.ease);
                    waitForAnimationIfRequested(command);
                    break;
                case VNCommandType::Wait:
                    waitRemainingSeconds = std::max(0.0f, command.durationSeconds);
                    if (waitRemainingSeconds > 0.0f)
                        status = VNRuntimeStatus::WaitingForTimer;
                    break;
                case VNCommandType::Choice:
                    executeChoice(command);
                    break;
                case VNCommandType::Jump:
                    jumpToLabel(command.label);
                    break;
                case VNCommandType::PlaySound:
                    executeCustom(ctx, "play_sound", command);
                    break;
                case VNCommandType::Custom:
                    executeCustom(ctx, command.commandName, command);
                    break;
            }
        }

        void executeSay(const VNCommand& command)
        {
            dialogue.visible = true;
            dialogue.speaker = command.speaker;
            dialogue.text = command.text;
            dialogue.visibleText.clear();
            dialogue.choices.clear();
            dialogue.visibleCharacters = 0;
            dialogue.typewriterAccumulator = 0.0f;
            dialogue.typewriterCharactersPerSecond =
                command.durationSeconds > 0.0f
                    ? static_cast<float>(command.text.size()) / command.durationSeconds
                    : config.typewriterCharactersPerSecond;
            dialogue.waitingForContinue = true;
            dialogue.continueIndicatorVisible = false;
            status = VNRuntimeStatus::WaitingForInput;
        }

        void executeShowSprite(const VNCommand& command)
        {
            VNSprite sprite;
            sprite.imageAsset = command.imageAsset;
            sprite.expression = command.expression;
            sprite.position = command.position;
            sprite.size = command.size;
            sprite.scale = command.scale;
            sprite.rotation = command.rotation;
            sprite.alpha = command.alpha;
            sprite.zOrder = command.zOrder;
            sprite.visible = true;

            stage.showSprite(command.target, sprite, command.durationSeconds, command.ease);
            waitForAnimationIfRequested(command);
        }

        void executeSetBackground(const VNCommand& command)
        {
            VNBackground background = command.background;
            if (!command.imageAsset.empty())
            {
                background.mode = VNBackgroundMode::FullscreenImage;
                background.imageAsset = command.imageAsset;
            }

            stage.setBackground(background);
        }

        void executeAnimateSprite(const VNCommand& command)
        {
            const std::string presetName = !command.presetName.empty() ? command.presetName : command.commandName;
            const VNSpriteMotionPreset* preset = motionProfile.findPreset(presetName);
            const VNSprite* sprite = stage.findSprite(command.target);
            if (preset == nullptr || sprite == nullptr)
                return;

            const float duration = command.durationSeconds > 0.0f
                ? command.durationSeconds
                : preset->durationSeconds;

            if (preset->positionOffset.x != 0.0f || preset->positionOffset.y != 0.0f)
                stage.moveSprite(command.target, sprite->position + preset->positionOffset, duration, preset->ease);

            if (preset->scaleOffset.x != 0.0f || preset->scaleOffset.y != 0.0f)
                stage.scaleSprite(command.target, sprite->scale + preset->scaleOffset, duration, preset->ease);

            if (preset->rotationOffset != 0.0f)
                stage.rotateSprite(command.target, sprite->rotation + preset->rotationOffset, duration, preset->ease);

            if (preset->alphaOffset != 0.0f)
                stage.fadeSprite(command.target,
                                 std::clamp(sprite->alpha + preset->alphaOffset, 0.0f, 1.0f),
                                 duration,
                                 preset->ease);

            if ((command.waitForCompletion || preset->waitForCompletion) && duration > 0.0f)
                status = VNRuntimeStatus::WaitingForAnimation;
        }

        void executeChoice(const VNCommand& command)
        {
            dialogue.visible = true;
            dialogue.speaker = command.speaker;
            dialogue.text = command.text;
            dialogue.visibleText = command.text;
            dialogue.visibleCharacters = command.text.size();
            dialogue.typewriterAccumulator = static_cast<float>(dialogue.visibleCharacters);
            dialogue.choices = command.choices;
            dialogue.waitingForContinue = false;
            dialogue.continueIndicatorVisible = false;
            status = VNRuntimeStatus::WaitingForChoice;
        }

        void executeCustom(const EcsControllerContext& ctx, const std::string& name, const VNCommand& command)
        {
            if (name.empty())
                return;

            VNCommandExecutionContext commandCtx{.runtime = *this, .stage = stage, .frame = ctx};
            const VNCommandResult result = commandRegistry.execute(name, commandCtx, command);

            if (result.stopScript)
            {
                completeScript();
                return;
            }

            if (!result.jumpLabel.empty())
                jumpToLabel(result.jumpLabel);

            if (!result.complete)
                status = VNRuntimeStatus::WaitingForExternalCommand;
        }

        void waitForAnimationIfRequested(const VNCommand& command)
        {
            if (command.waitForCompletion && command.durationSeconds > 0.0f)
                status = VNRuntimeStatus::WaitingForAnimation;
        }

        void completeScript()
        {
            status = VNRuntimeStatus::Complete;
            cursor = script.commands.size();
            waitRemainingSeconds = 0.0f;
            dialogue.clear();
        }
    };
} // namespace gts::vn
