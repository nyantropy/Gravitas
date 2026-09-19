#pragma once

#include "SceneExecutionProfile.h"

namespace gts::vn
{
    inline SceneExecutionProfile dialogueOverlay()
    {
        return {"dialogue_overlay",
                gts::execution::groups::Always | gts::execution::groups::Camera | gts::execution::groups::RenderPrep |
                    gts::execution::groups::Ui | gts::execution::groups::Dialogue | gts::execution::groups::VN |
                    gts::execution::groups::Audio | gts::execution::groups::Tools,
                FrameBuildMode::FullWorld,
                TimePolicy::GameplayPausedUiRunning};
    }

    inline SceneExecutionProfile fullscreenDialogue()
    {
        return {"fullscreen_dialogue",
                gts::execution::groups::Always | gts::execution::groups::Ui | gts::execution::groups::Dialogue |
                    gts::execution::groups::VN | gts::execution::groups::Audio | gts::execution::groups::Tools,
                FrameBuildMode::UiOnly,
                TimePolicy::GameplayPausedDialogueRunning};
    }

} // namespace gts::vn
