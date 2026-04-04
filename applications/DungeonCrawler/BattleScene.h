#pragma once

#include <string>

#include "Entity.h"
#include "GlmConfig.h"
#include "GtsScene.hpp"

class BattleScene : public GtsScene
{
public:
    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override;
    void onUpdateSimulation(SceneContext& ctx) override;
    void onUpdateControllers(SceneContext& ctx) override;

private:
    Entity      enemyEntity      = INVALID_ENTITY;
    Entity      floorEntity      = INVALID_ENTITY;
    Entity      cameraEntity     = INVALID_ENTITY;
    std::string returnSceneName  = "dungeon_test";
    int         returnFloorIndex = -1;
    glm::ivec2  returnPlayerTile = {-1, -1};
    glm::ivec2  defeatedEnemyTile = {-1, -1};
};
