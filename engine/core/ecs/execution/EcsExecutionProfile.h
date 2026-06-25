#pragma once

#include <cstdint>
#include <string>
#include <string_view>

enum class EcsSystemGroup : uint64_t
{
    Always    = 1ull << 0,
    Gameplay  = 1ull << 1,
    Physics   = 1ull << 2,
    Camera    = 1ull << 3,
    RenderPrep = 1ull << 4,
    Particles = 1ull << 5,
    Animation = 1ull << 6,
    Audio     = 1ull << 7,
    Ui        = 1ull << 8,
    Dialogue  = 1ull << 9,
    VN        = 1ull << 10,
    Tools     = 1ull << 11
};

using EcsSystemMask = uint64_t;

constexpr EcsSystemMask toMask(EcsSystemGroup group)
{
    return static_cast<EcsSystemMask>(group);
}

constexpr EcsSystemMask operator|(EcsSystemGroup lhs, EcsSystemGroup rhs)
{
    return toMask(lhs) | toMask(rhs);
}

constexpr EcsSystemMask operator|(EcsSystemMask lhs, EcsSystemGroup rhs)
{
    return lhs | toMask(rhs);
}

constexpr EcsSystemMask operator|(EcsSystemGroup lhs, EcsSystemMask rhs)
{
    return toMask(lhs) | rhs;
}

constexpr bool containsSystemGroup(EcsSystemMask mask, EcsSystemGroup group)
{
    return (mask & toMask(group)) != 0;
}

enum class RenderBuildMode
{
    FullWorld,
    UiOnly,
    CachedWorldFrame,
    None
};

enum class TimePolicy
{
    AllRunning,
    GameplayPausedUiRunning,
    GameplayPausedDialogueRunning,
    EverythingPaused
};

struct SceneExecutionProfile
{
    std::string id = "gameplay";
    EcsSystemMask enabledSystems = 0;
    RenderBuildMode renderBuildMode = RenderBuildMode::FullWorld;
    TimePolicy timePolicy = TimePolicy::AllRunning;

    bool contains(EcsSystemGroup group) const
    {
        return containsSystemGroup(enabledSystems, group);
    }

    static SceneExecutionProfile gameplay()
    {
        return {
            "gameplay",
            EcsSystemGroup::Always
                | EcsSystemGroup::Gameplay
                | EcsSystemGroup::Physics
                | EcsSystemGroup::Camera
                | EcsSystemGroup::RenderPrep
                | EcsSystemGroup::Particles
                | EcsSystemGroup::Animation
                | EcsSystemGroup::Audio
                | EcsSystemGroup::Ui
                | EcsSystemGroup::Dialogue
                | EcsSystemGroup::VN
                | EcsSystemGroup::Tools,
            RenderBuildMode::FullWorld,
            TimePolicy::AllRunning
        };
    }

    static SceneExecutionProfile dialogueOverlay()
    {
        return {
            "dialogue_overlay",
            EcsSystemGroup::Always
                | EcsSystemGroup::Camera
                | EcsSystemGroup::RenderPrep
                | EcsSystemGroup::Ui
                | EcsSystemGroup::Dialogue
                | EcsSystemGroup::VN
                | EcsSystemGroup::Audio
                | EcsSystemGroup::Tools,
            RenderBuildMode::FullWorld,
            TimePolicy::GameplayPausedUiRunning
        };
    }

    static SceneExecutionProfile fullscreenDialogue()
    {
        return {
            "fullscreen_dialogue",
            EcsSystemGroup::Always
                | EcsSystemGroup::Ui
                | EcsSystemGroup::Dialogue
                | EcsSystemGroup::VN
                | EcsSystemGroup::Audio
                | EcsSystemGroup::Tools,
            RenderBuildMode::UiOnly,
            TimePolicy::GameplayPausedDialogueRunning
        };
    }

    static SceneExecutionProfile pauseMenu()
    {
        return {
            "pause_menu",
            EcsSystemGroup::Always
                | EcsSystemGroup::Ui
                | EcsSystemGroup::Audio
                | EcsSystemGroup::Tools,
            RenderBuildMode::CachedWorldFrame,
            TimePolicy::GameplayPausedUiRunning
        };
    }
};
