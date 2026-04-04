#pragma once

#include "GlmConfig.h"
#include "GtsSceneTransitionData.h"

struct BattleResultTransitionData : public GtsSceneTransitionData
{
    int        floorIndex        = -1;
    glm::ivec2 playerTile        = {-1, -1};
    glm::ivec2 defeatedEnemyTile = {-1, -1};
};
