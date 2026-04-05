#pragma once

#include <limits>
#include <vector>

#include "Entity.h"
#include "GtsFrameStats.h"
#include "GtsScene.hpp"
#include "Types.h"

#include "DungeonManager.h"
#include "DungeonVisibilityState.h"
#include "generator/GeneratedFloor.h"
#include "inventory/CollectibleRunState.h"
#include "inventory/Item.h"
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
    void populateFrameStats(GtsFrameStats& stats) const override;

private:
    DungeonManager       dungeon;
    std::vector<Entity>  floorEntities;
    Entity               playerEntity = Entity{ std::numeric_limits<entity_id_type>::max() };
    Entity               playerMarkerEntity = Entity{ std::numeric_limits<entity_id_type>::max() };
    DungeonUiController  uiController;
    DungeonVisibilityState visibilityState;
    bool                 runInitialized = false;
    bool                 stairLatchActive = false;
    int                  latchedFloorIndex = -1;
    glm::ivec2           latchedStairPos   = {-1, -1};
    std::vector<Item>    persistentInventoryItems;
    uint32_t             persistentGoldAmount = 0;
    CollectibleRunState  persistentCollectibles;

    void initializeDungeonSingleton();
    void rebuildActiveFloor(SceneContext& ctx);
    void buildFloorEntities(const GeneratedFloor& floor);
    void spawnEnemyEntities(const GeneratedFloor& floor);
    void spawnCollectibles(const GeneratedFloor& floor);
    void destroyFloorEntities();
    void spawnStairFeature(const GeneratedFloor& floor, const glm::ivec2& stairPos, bool descends);
    void spawnPlayer(SceneContext& ctx, const glm::ivec2& startPos);
    void spawnPlayerMarker();
    void movePlayerToTile(const glm::ivec2& gridPos);
    void syncPlayerMarker();
    void beginFloorTransition(SceneContext& ctx, bool movingDown, const glm::ivec2& sourceGridPos);
    void updateFloorTransition(SceneContext& ctx);
    void updateMinimapReveal(SceneContext& ctx);
    void handleDungeonRegenerate(SceneContext& ctx);
    void handleEncounterTransitions(SceneContext& ctx);
    void handleStairTransitions(SceneContext& ctx);
    void updateStairLatch(const GeneratedFloor& floor, const glm::ivec2& playerGridPos);
    void removeDefeatedEnemy(int floorIndex, const glm::ivec2& spawnTile);
    void chooseNewCollectibleRunState();
    void syncPersistentPlayerStateFromWorld();
    DungeonUiState buildUiState();
};
