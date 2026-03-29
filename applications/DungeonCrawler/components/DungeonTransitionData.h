#pragma once

#include "GtsSceneTransitionData.h"
#include "GlmConfig.h"

// Scene-transition payload passed when the player uses a staircase.
// DungeonFloorScene::onLoad reads this to know where to spawn the player.
//
// playerGridPos: the grid cell the player entered from ({-1,-1} = use default start).
// fromFloor:     the floor number the player is coming from (-1 = unset).
struct DungeonTransitionData : public GtsSceneTransitionData
{
    glm::ivec2 playerGridPos = {-1, -1};
    int        fromFloor     = -1;
};
