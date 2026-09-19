#pragma once

#include "SceneExecutionProfile.h"
#include "VNExecutionInputs.h"

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

    inline VNExecutionInputs executionInputs()
    {
        return {SceneExecutionProfile::gameplay(), dialogueOverlay(), fullscreenDialogue()};
    }

} // namespace gts::vn
