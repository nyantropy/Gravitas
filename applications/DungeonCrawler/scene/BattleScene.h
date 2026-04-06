#pragma once

#include <limits>
#include <string>

#include "Entity.h"
#include "GlmConfig.h"
#include "GtsScene.hpp"
#include "InputActionManager.hpp"
#include "utils/DungeonAction.h"

class BattleScene : public GtsScene
{
public:
    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override;
    void onUpdateSimulation(SceneContext& ctx) override;
    void onUpdateControllers(SceneContext& ctx) override;

private:
    InputActionManager<DungeonAction> actionManager;
    Entity      enemyEntity      = Entity{std::numeric_limits<entity_id_type>::max()};
    Entity      floorEntity      = Entity{std::numeric_limits<entity_id_type>::max()};
    Entity      cameraEntity     = Entity{std::numeric_limits<entity_id_type>::max()};
    std::string returnSceneName  = "dungeon_test";
    int         returnFloorIndex = -1;
    glm::ivec2  returnPlayerTile = {-1, -1};
    glm::ivec2  defeatedEnemyTile = {-1, -1};
};
