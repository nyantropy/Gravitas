#pragma once

#include <limits>
#include <vector>

#include "Entity.h"
#include "GtsScene.hpp"
#include "Types.h"

#include "DungeonManager.h"
#include "DungeonVisibilityState.h"
#include "GeneratedFloor.h"
#include "ui/DungeonUiController.h"
#include "ui/DungeonUiState.h"

class DungeonTestScene : public GtsScene
{
public:
    DungeonTestScene();

    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override;
    void onUpdateSimulation(SceneContext& ctx) override;
    void onUpdateControllers(SceneContext& ctx) override;

private:
    DungeonManager       dungeon;
    std::vector<Entity>  floorEntities;
    Entity               playerEntity = Entity{ std::numeric_limits<entity_id_type>::max() };
    Entity               playerMarkerEntity = Entity{ std::numeric_limits<entity_id_type>::max() };
    DungeonUiController  uiController;
    DungeonVisibilityState visibilityState;
    bool                 stairLatchActive = false;
    int                  latchedFloorIndex = -1;
    glm::ivec2           latchedStairPos   = {-1, -1};

    void initializeDungeonSingleton();
    void rebuildActiveFloor(SceneContext& ctx);
    void buildFloorEntities(const GeneratedFloor& floor);
    void destroyFloorEntities();
    void spawnPlayer(SceneContext& ctx, const glm::ivec2& startPos);
    void spawnPlayerMarker();
    void movePlayerToTile(const glm::ivec2& gridPos);
    void syncPlayerMarker();
    void updateMinimapReveal(SceneContext& ctx);
    void handleDungeonRegenerate(SceneContext& ctx);
    void handleStairTransitions(SceneContext& ctx);
    void updateStairLatch(const GeneratedFloor& floor, const glm::ivec2& playerGridPos);
    DungeonUiState buildUiState();
};
