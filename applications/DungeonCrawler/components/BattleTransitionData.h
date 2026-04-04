#pragma once

#include <string>

#include "GlmConfig.h"
#include "GtsSceneTransitionData.h"

struct BattleTransitionData : public GtsSceneTransitionData
{
    std::string returnSceneName = "dungeon_test";
    int         floorIndex      = -1;
    glm::ivec2  playerTile      = {-1, -1};
    glm::ivec2  enemyTile       = {-1, -1};
    glm::ivec2  enemySpawnTile  = {-1, -1};
};
