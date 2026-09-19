#include "VNExecutionProfiles.h"
#include "SceneExecutionPolicy.h"
#include "BuiltinExecutionGroups.h"
#include "ECSWorld.hpp"
#include <array>
#include <stdexcept>
#include <string_view>

void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
int main()
{
    const std::array groups{gts::execution::groups::Always,
                            gts::execution::groups::Gameplay,
                            gts::execution::groups::Physics,
                            gts::execution::groups::Camera,
                            gts::execution::groups::RenderPrep,
                            gts::execution::groups::Particles,
                            gts::execution::groups::Animation,
                            gts::execution::groups::Audio,
                            gts::execution::groups::Ui,
                            gts::execution::groups::Dialogue,
                            gts::execution::groups::VN,
                            gts::execution::groups::Tools};
    const std::array names{"Always",
                           "Gameplay",
                           "Physics",
                           "Camera",
                           "RenderPrep",
                           "Particles",
                           "Animation",
                           "Audio",
                           "Ui",
                           "Dialogue",
                           "VN",
                           "Tools"};
    for (size_t i = 0; i < groups.size(); ++i)
        require(toMask(groups[i]) == (1ull << i) && std::string_view(ecsSystemGroupName(groups[i])) == names[i],
                "Fixed bit identity or diagnostic label changed");
    require(std::string_view(ecsSystemGroupName(static_cast<EcsSystemGroup>(1ull << 44))) == "Unknown",
            "Unknown label");
    const std::array profiles{SceneExecutionProfile::gameplay(),
                              gts::vn::dialogueOverlay(),
                              gts::vn::fullscreenDialogue(),
                              SceneExecutionProfile::pauseMenu()};
    const std::array masks{0xfffull, 0xf99ull, 0xf81ull, 0x981ull};
    const std::array ids{"gameplay", "dialogue_overlay", "fullscreen_dialogue", "pause_menu"};
    const std::array modes{
        FrameBuildMode::FullWorld, FrameBuildMode::FullWorld, FrameBuildMode::UiOnly, FrameBuildMode::CachedWorldFrame};
    const std::array times{TimePolicy::AllRunning,
                           TimePolicy::GameplayPausedUiRunning,
                           TimePolicy::GameplayPausedDialogueRunning,
                           TimePolicy::GameplayPausedUiRunning};
    ECSWorld         world;
    gts::execution::ensureExecutionPolicy(world);
    for (size_t i = 0; i < profiles.size(); ++i)
    {
        require(profiles[i].enabledSystems == masks[i] && profiles[i].id == ids[i] &&
                    profiles[i].frameBuildMode == modes[i] && profiles[i].timePolicy == times[i],
                "Preset contract changed");
        world.pushExecutionSelection(profiles[i]);
        require(world.getCurrentExecutionSelection().id == ids[i], "Profile identity changed");
    }
    require(gts::execution::sceneExecutionPolicy(world).frameBuildMode == FrameBuildMode::CachedWorldFrame,
            "Top policy metadata must match the mask selection");
    require(!world.popExecutionSelection("fullscreen_dialogue"), "Guarded pop must leave metadata and mask together");
    world.popExecutionSelection("pause_menu");
    require(gts::execution::sceneExecutionPolicy(world).frameBuildMode == FrameBuildMode::UiOnly &&
                world.getCurrentExecutionSelection().enabledSystems == 0xf81,
            "Pop must restore both mask and presentation");
    world.clear();
    require(world.getExecutionSelectionDepth() == 1 && world.getCurrentExecutionSelection().id == "gameplay" &&
                world.getCurrentExecutionSelection().enabledSystems == 0xfff,
            "Default/reset policy changed");
    require(!world.shouldExecuteGroup(static_cast<EcsSystemGroup>(1ull << 44)), "Default must not enable unknown bits");
    require(SceneExecutionProfile{}.enabledSystems == 0, "Value-initialized profile differs from gameplay factory");
    auto empty           = SceneExecutionProfile::gameplay();
    empty.enabledSystems = 0;
    empty.timePolicy     = TimePolicy::EverythingPaused;
    empty.frameBuildMode = FrameBuildMode::None;
    world.pushExecutionSelection(empty);
    empty.frameBuildMode = FrameBuildMode::FullWorld;
    empty.timePolicy     = TimePolicy::AllRunning;
    require(gts::execution::sceneExecutionPolicy(world).frameBuildMode == FrameBuildMode::None &&
                gts::execution::sceneExecutionPolicy(world).timePolicy == TimePolicy::EverythingPaused,
            "Policy metadata must be value owned by its stack entry");
    ECSWorld isolated;
    gts::execution::ensureExecutionPolicy(isolated);
    require(isolated.getCurrentExecutionSelection().enabledSystems == 0xfff &&
                gts::execution::sceneExecutionPolicy(isolated).frameBuildMode == FrameBuildMode::FullWorld,
            "World policy isolation");
    gts::execution::ensureExecutionPolicy(world);
    require(world.getExecutionSelectionDepth() == 2 && world.getCurrentExecutionSelection().enabledSystems == 0,
            "Repeated installation must not replace an active selection");
    require(!world.shouldExecuteGroup(gts::execution::groups::Always), "Always must remain maskable");
}
