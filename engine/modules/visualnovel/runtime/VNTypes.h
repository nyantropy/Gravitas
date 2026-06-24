#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "GlmConfig.h"
#include "Tween.h"

namespace gts::vn
{
    enum class VNBackgroundMode
    {
        UseCurrentScene = 0,
        FullscreenImage,
        SolidColor,
        None
    };

    struct VNBackground
    {
        VNBackgroundMode mode = VNBackgroundMode::UseCurrentScene;
        std::string imageAsset;
        glm::vec4 color = {0.0f, 0.0f, 0.0f, 1.0f};
    };

    struct VNSprite
    {
        std::string imageAsset;
        std::string expression;
        glm::vec2 position = {0.5f, 0.5f};
        glm::vec2 size = {0.28f, 0.72f};
        glm::vec2 scale = {1.0f, 1.0f};
        float rotation = 0.0f;
        float alpha = 1.0f;
        int zOrder = 0;
        bool visible = true;
    };

    struct VNSpriteView
    {
        std::string id;
        VNSprite sprite;
    };

    struct VNChoiceOption
    {
        std::string label;
        std::string targetLabel;
    };

    struct VNDialogueState
    {
        bool visible = false;
        std::string speaker;
        std::string text;
        std::string visibleText;
        std::vector<VNChoiceOption> choices;
        float typewriterCharactersPerSecond = 48.0f;
        float typewriterAccumulator = 0.0f;
        size_t visibleCharacters = 0;
        bool waitingForContinue = false;
        bool continueIndicatorVisible = false;

        void clear()
        {
            *this = VNDialogueState{};
        }
    };

    enum class VNCommandType
    {
        Say = 0,
        ShowSprite,
        HideSprite,
        MoveSprite,
        FadeSprite,
        ScaleSprite,
        RotateSprite,
        ShakeSprite,
        ChangeExpression,
        SetBackground,
        SetDimming,
        Wait,
        Choice,
        Jump,
        PlaySound,
        Custom
    };

    struct VNCommand
    {
        VNCommandType type = VNCommandType::Custom;
        std::string commandName;
        std::string target;
        std::string speaker;
        std::string text;
        std::string imageAsset;
        std::string expression;
        std::string label;
        std::string soundAsset;
        VNBackground background;
        glm::vec2 position = {0.5f, 0.5f};
        glm::vec2 scale = {1.0f, 1.0f};
        glm::vec2 size = {0.28f, 0.72f};
        float rotation = 0.0f;
        float alpha = 1.0f;
        float durationSeconds = 0.0f;
        float intensity = 0.012f;
        float frequency = 18.0f;
        int zOrder = 0;
        bool waitForCompletion = false;
        gts::tween::TweenEase ease = gts::tween::TweenEase::SmoothStep;
        std::vector<std::string> args;
        std::vector<VNChoiceOption> choices;
    };

    struct VNScript
    {
        std::vector<VNCommand> commands;
        std::unordered_map<std::string, size_t> labels;

        void addLabel(const std::string& label)
        {
            labels[label] = commands.size();
        }

        void add(const VNCommand& command)
        {
            commands.push_back(command);
        }

        bool findLabel(const std::string& label, size_t& outCursor) const
        {
            const auto it = labels.find(label);
            if (it == labels.end())
                return false;

            outCursor = it->second;
            return true;
        }
    };

    struct VNRuntimeConfig
    {
        std::string continueAction = "engine.ui_primary";
        float typewriterCharactersPerSecond = 48.0f;
        bool blockGameplayInput = true;
        bool capturePointerInput = true;
    };
} // namespace gts::vn
