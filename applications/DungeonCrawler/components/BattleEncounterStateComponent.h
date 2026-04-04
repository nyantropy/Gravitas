#pragma once

#include "GlmConfig.h"

struct BattleEncounterStateComponent
{
    bool       battleRequested      = false;
    bool       transitionIssued     = false;
    uint32_t   playerCollisionCount = 0;
    int        floorIndex           = -1;
    glm::ivec2 playerTile           = {-1, -1};
    glm::ivec2 enemyTile            = {-1, -1};
    glm::ivec2 enemySpawnTile       = {-1, -1};
};
