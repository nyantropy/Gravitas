#pragma once

#include <limits>
#include <vector>

#include "Entity.h"
#include "GtsFrameStats.h"
#include "GtsScene.hpp"

#include "dungeon/DungeonManager.h"
#include "dungeon/DungeonSpawner.h"
#include "dungeon/DungeonTransitionSystem.h"
#include "dungeon/DungeonVisibilityState.h"
#include "enemy/EnemySpawner.h"
#include "inventory/CollectibleRunState.h"
#include "inventory/CollectibleSpawner.h"
#include "inventory/Item.h"
#include "player/PlayerMarkerSystem.h"
#include "player/PlayerSpawner.h"
#include "player/PlayerStateSync.h"
#include "ui/DungeonUiBuilder.h"
#include "ui/DungeonUiController.h"

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
    DungeonManager          dungeon;
    DungeonSpawner          dungeonSpawner;
    DungeonTransitionSystem transitionSystem;
    EnemySpawner            enemySpawner;
    CollectibleSpawner      collectibleSpawner;
    PlayerSpawner           playerSpawner;
    PlayerMarkerSystem      playerMarkerSystem;
    PlayerStateSync         playerStateSync;
    DungeonUiBuilder        uiBuilder;
    DungeonUiController     uiController;
    DungeonVisibilityState  visibilityState;

    std::vector<Entity> floorEntities;
    Entity              playerEntity = Entity{std::numeric_limits<entity_id_type>::max()};
    Entity              playerMarkerEntity = Entity{std::numeric_limits<entity_id_type>::max()};
    bool                runInitialized = false;

    std::vector<Item>   persistentInventoryItems;
    uint32_t            persistentGoldAmount = 0;
    CollectibleRunState persistentCollectibles;

    void initializeRunState(const GtsSceneTransitionData* data, glm::ivec2& playerStart);
    void initializeDungeonSingleton();
    void rebuildActiveFloor();
    void updateMinimapReveal();
    void handleDungeonRegenerate();
    void handleEncounterTransitions(SceneContext& ctx);
    void syncPersistentWorldState();
};
