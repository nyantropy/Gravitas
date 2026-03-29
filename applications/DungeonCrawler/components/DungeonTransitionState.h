#pragma once

#include "GlmConfig.h"

// Global transition state written by StairInteractionSystem and read by
// DungeonFloorScene::onLoad to position the player at the correct staircase.
//
// Using a simple static instance avoids the need to modify GtsCommandBuffer
// or the engine itself. The owning DungeonFloorScene clears / ignores stale
// values by checking fromFloor against the scene's own floorNumber.
struct DungeonTransitionState
{
    glm::ivec2 playerGridPos = {-1, -1};
    int        fromFloor     = -1;

    static DungeonTransitionState& instance()
    {
        static DungeonTransitionState s;
        return s;
    }

private:
    DungeonTransitionState() = default;
};
