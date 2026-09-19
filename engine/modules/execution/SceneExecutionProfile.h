#pragma once

#include "BuiltinExecutionGroups.h"

enum class FrameBuildMode
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

// Presentation/time metadata is interpreted outside ECS. TimePolicy remains
// descriptive only; it does not change clocks or engine pause.
struct SceneExecutionPolicy
{
    FrameBuildMode frameBuildMode = FrameBuildMode::FullWorld;
    TimePolicy     timePolicy     = TimePolicy::AllRunning;
};

struct SceneExecutionProfile
{
    std::string    id             = "gameplay";
    EcsSystemMask  enabledSystems = 0;
    FrameBuildMode frameBuildMode = FrameBuildMode::FullWorld;
    TimePolicy     timePolicy     = TimePolicy::AllRunning;

    operator EcsExecutionSelection() const
    {
        return {id, enabledSystems, SceneExecutionPolicy{frameBuildMode, timePolicy}};
    }

    bool contains(EcsSystemGroup group) const
    {
        return containsSystemGroup(enabledSystems, group);
    }

    static SceneExecutionProfile gameplay()
    {
        return {"gameplay",
                gts::execution::groups::Always | gts::execution::groups::Gameplay | gts::execution::groups::Physics |
                    gts::execution::groups::Camera | gts::execution::groups::RenderPrep |
                    gts::execution::groups::Particles | gts::execution::groups::Animation |
                    gts::execution::groups::Audio | gts::execution::groups::Ui | gts::execution::groups::Dialogue |
                    gts::execution::groups::VN | gts::execution::groups::Tools,
                FrameBuildMode::FullWorld,
                TimePolicy::AllRunning};
    }

    static SceneExecutionProfile pauseMenu()
    {
        return {"pause_menu",
                gts::execution::groups::Always | gts::execution::groups::Ui | gts::execution::groups::Audio |
                    gts::execution::groups::Tools,
                FrameBuildMode::CachedWorldFrame,
                TimePolicy::GameplayPausedUiRunning};
    }
};
