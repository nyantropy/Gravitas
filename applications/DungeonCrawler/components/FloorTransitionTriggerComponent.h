#pragma once

#include "GlmConfig.h"
#include "components/FloorTransitionType.h"

// Attach to a tile entity to make it a floor transition trigger.
// When the player stands on this tile and is not mid-transition,
// FloorTransitionSystem initiates the transition.
//
// arrivalGrid: grid position on the destination floor where the player
// lands. Set to {-1,-1} to fall back to dest->playerStart.
// Must be one step past the arrival stair tile so the player does not
// immediately re-trigger the transition on arrival.
struct FloorTransitionTriggerComponent
{
    FloorTransitionType type          = FloorTransitionType::Stairs;
    int                 targetFloor   = 0;
    glm::ivec2          arrivalGrid   = {-1, -1};
    bool                bidirectional = true;
};
