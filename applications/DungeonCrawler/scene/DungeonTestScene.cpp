#include "scene/DungeonTestScene.h"

#include <limits>
#include <memory>

#include "DebugCameraStateComponent.h"
#include "components/BattleEncounterStateComponent.h"
#include "components/BattleResultTransitionData.h"
#include "components/BattleTransitionData.h"
#include "components/DungeonFloorSingleton.h"
#include "components/DungeonInputComponent.h"
#include "components/FloorTransitionStateComponent.h"
#include "components/PlayerComponent.h"
#include "inventory/InventoryEventSystem.hpp"
#include "inventory/InventoryPickupSystem.hpp"
#include "inventory/InventoryUiSystem.hpp"
#include "inventory/KeyRotationSystem.hpp"
#include "systems/DungeonInputSystem.hpp"
#include "systems/EnemyInteractionSystem.hpp"
#include "systems/EnemyMovementSystem.hpp"
#include "systems/PlayerCameraSystem.h"
#include "systems/PlayerMovementSystem.hpp"

DungeonTestScene::DungeonTestScene()
    : dungeon(0x12345u, 4)
{
}

void DungeonTestScene::onLoad(SceneContext& ctx, const GtsSceneTransitionData* data)
{
    constexpr Entity invalidEntity{std::numeric_limits<entity_id_type>::max()};

    resetSceneWorld();
    floorEntities.clear();
    playerEntity = invalidEntity;
    playerMarkerEntity = invalidEntity;
    uiController.reset();
    transitionSystem.reset();

    glm::ivec2 playerStart = {1, 1};
    initializeRunState(data, playerStart);

    ecsWorld.createSingleton<DungeonInputComponent>();
    ecsWorld.createSingleton<FloorTransitionStateComponent>();
    ecsWorld.createSingleton<BattleEncounterStateComponent>();
    ecsWorld.createSingleton<CollectibleRunState>(persistentCollectibles);

    initializeDungeonSingleton();
    rebuildActiveFloor();

    playerEntity = playerSpawner.spawnPlayer(ecsWorld,
                                             ctx,
                                             playerStart,
                                             persistentInventoryItems,
                                             persistentGoldAmount);
    playerMarkerEntity = playerMarkerSystem.spawnMarker(ecsWorld);
    playerMarkerSystem.syncMarker(ecsWorld, playerEntity, playerMarkerEntity);
    uiController.initialize(ctx, uiBuilder.buildState(dungeon, visibilityState, ecsWorld, playerEntity));

    ecsWorld.addControllerSystem<DungeonInputSystem>();
    ecsWorld.addControllerSystem<PlayerMovementSystem>();
    ecsWorld.addControllerSystem<PlayerCameraSystem>();
    ecsWorld.addControllerSystem<KeyRotationSystem>();

    installPhysicsFeature(ctx);

    ecsWorld.addControllerSystem<InventoryPickupSystem>(ctx.physics);
    ecsWorld.addControllerSystem<InventoryEventSystem>();
    ecsWorld.addControllerSystem<InventoryUiSystem>();
    ecsWorld.addSimulationSystem<EnemyMovementSystem>();
    ecsWorld.addSimulationSystem<EnemyInteractionSystem>(ctx.physics);

    installRendererFeature(ctx);
}

void DungeonTestScene::onUpdateSimulation(SceneContext& ctx)
{
    ecsWorld.updateSimulation(ctx.time->deltaTime);
}

void DungeonTestScene::onUpdateControllers(SceneContext& ctx)
{
    ecsWorld.updateControllers(ctx);

    handleEncounterTransitions(ctx);
    if (ecsWorld.hasAny<BattleEncounterStateComponent>()
        && ecsWorld.getSingleton<BattleEncounterStateComponent>().transitionIssued)
    {
        playerMarkerSystem.syncMarker(ecsWorld, playerEntity, playerMarkerEntity);
        uiController.update(ctx, uiBuilder.buildState(dungeon, visibilityState, ecsWorld, playerEntity));
        return;
    }

    handleDungeonRegenerate();

    transitionSystem.update(
        ctx,
        ecsWorld,
        dungeon,
        visibilityState,
        playerEntity,
        [this]()
        {
            rebuildActiveFloor();
        },
        [this](const glm::ivec2& gridPos)
        {
            playerStateSync.movePlayerToTile(ecsWorld, playerEntity, gridPos);
        });

    updateMinimapReveal();
    playerMarkerSystem.syncMarker(ecsWorld, playerEntity, playerMarkerEntity);
    syncPersistentWorldState();
    uiController.update(ctx, uiBuilder.buildState(dungeon, visibilityState, ecsWorld, playerEntity));
}

void DungeonTestScene::populateFrameStats(GtsFrameStats& stats) const
{
    stats.minimapCellCount = uiController.getLastMinimapCellCount();

    if (physicsWorld)
        stats.physicsCollisionCount = static_cast<uint32_t>(physicsWorld->getCollisions().size());

    if (ecsWorld.hasAny<BattleEncounterStateComponent>())
    {
        const auto& encounter = ecsWorld.getSingleton<BattleEncounterStateComponent>();
        stats.playerCollisionCount = encounter.playerCollisionCount;
    }
}

void DungeonTestScene::initializeRunState(const GtsSceneTransitionData* data, glm::ivec2& playerStart)
{
    const auto* battleResult = dynamic_cast<const BattleResultTransitionData*>(data);
    if (battleResult != nullptr
        && runInitialized
        && dungeon.canMoveToFloor(battleResult->floorIndex))
    {
        dungeon.setCurrentFloorIndex(battleResult->floorIndex);
        enemySpawner.removeDefeatedEnemy(dungeon, battleResult->floorIndex, battleResult->defeatedEnemyTile);
        playerStart = battleResult->playerTile;
        visibilityState.revealAt(dungeon.getCurrentFloorIndex(),
                                 dungeon.getActiveFloor(),
                                 playerStart.x,
                                 playerStart.y);
        return;
    }

    dungeon.generateRun();
    visibilityState.initialize(dungeon.getFloors());
    dungeon.setCurrentFloorIndex(0);
    playerStart = dungeon.getActiveFloor().playerStart;
    visibilityState.revealAt(0,
                             dungeon.getActiveFloor(),
                             playerStart.x,
                             playerStart.y);

    persistentInventoryItems.clear();
    persistentGoldAmount = 0;
    collectibleSpawner.chooseNewCollectibleRunState(dungeon, persistentCollectibles);
    runInitialized = true;
}

void DungeonTestScene::initializeDungeonSingleton()
{
    auto& floorSingleton = ecsWorld.createSingleton<DungeonFloorSingleton>();
    floorSingleton.run = &dungeon;

    const auto& floors = dungeon.getFloors();
    for (int i = 0; i < dungeon.getTotalFloorCount(); ++i)
    {
        floorSingleton.allFloors[i] = &floors[static_cast<size_t>(i)];
        floorSingleton.floorWorldOffset[i] = {0.0f, 0.0f, 0.0f};
    }

    floorSingleton.floor = &dungeon.getActiveFloor();
    floorSingleton.currentFloorIndex = dungeon.getCurrentFloorIndex();
}

void DungeonTestScene::rebuildActiveFloor()
{
    dungeonSpawner.destroyFloor(ecsWorld, floorEntities);

    auto& floorSingleton = ecsWorld.getSingleton<DungeonFloorSingleton>();
    floorSingleton.floor = &dungeon.getActiveFloor();
    floorSingleton.currentFloorIndex = dungeon.getCurrentFloorIndex();

    const auto& floors = dungeon.getFloors();
    for (int i = 0; i < dungeon.getTotalFloorCount(); ++i)
        floorSingleton.allFloors[i] = &floors[static_cast<size_t>(i)];

    dungeonSpawner.buildFloor(ecsWorld, floorEntities, dungeon.getActiveFloor());
    enemySpawner.spawnEnemies(ecsWorld, floorEntities, dungeon.getActiveFloor());

    if (ecsWorld.hasAny<CollectibleRunState>())
        collectibleSpawner.spawnCollectibles(ecsWorld,
                                             floorEntities,
                                             dungeon.getActiveFloor(),
                                             ecsWorld.getSingleton<CollectibleRunState>());
}

void DungeonTestScene::updateMinimapReveal()
{
    constexpr Entity invalidEntity{std::numeric_limits<entity_id_type>::max()};
    if (playerEntity == invalidEntity)
        return;

    const auto& input = ecsWorld.getSingleton<DungeonInputComponent>();
    if (input.toggleMinimapRevealPressed)
        visibilityState.toggleRevealMode();

    const PlayerComponent& player = ecsWorld.getComponent<PlayerComponent>(playerEntity);
    visibilityState.revealAt(dungeon.getCurrentFloorIndex(),
                             dungeon.getActiveFloor(),
                             player.gridX,
                             player.gridZ);
}

void DungeonTestScene::handleDungeonRegenerate()
{
    if (ecsWorld.getSingleton<FloorTransitionStateComponent>().active)
        return;

    const auto& input = ecsWorld.getSingleton<DungeonInputComponent>();
    if (!input.regeneratePressed)
        return;

    dungeon.generateRun();
    visibilityState.initialize(dungeon.getFloors());
    dungeon.setCurrentFloorIndex(0);
    persistentInventoryItems.clear();
    persistentGoldAmount = 0;
    collectibleSpawner.chooseNewCollectibleRunState(dungeon, persistentCollectibles);
    visibilityState.revealAt(0,
                             dungeon.getActiveFloor(),
                             dungeon.getActiveFloor().playerStart.x,
                             dungeon.getActiveFloor().playerStart.y);

    if (ecsWorld.hasAny<CollectibleRunState>())
        ecsWorld.getSingleton<CollectibleRunState>() = persistentCollectibles;

    rebuildActiveFloor();
    playerStateSync.movePlayerToTile(ecsWorld, playerEntity, dungeon.getActiveFloor().playerStart);
    transitionSystem.reset();
}

void DungeonTestScene::handleEncounterTransitions(SceneContext& ctx)
{
    if (!ecsWorld.hasAny<BattleEncounterStateComponent>())
        return;

    auto& encounter = ecsWorld.getSingleton<BattleEncounterStateComponent>();
    if (!encounter.battleRequested || encounter.transitionIssued)
        return;

    auto transition = std::make_shared<BattleTransitionData>();
    transition->returnSceneName = "dungeon_test";
    transition->floorIndex = encounter.floorIndex;
    transition->playerTile = encounter.playerTile;
    transition->enemyTile = encounter.enemyTile;
    transition->enemySpawnTile = encounter.enemySpawnTile;

    encounter.transitionIssued = true;
    ctx.engineCommands->requestChangeScene("battle", transition);
}

void DungeonTestScene::syncPersistentWorldState()
{
    playerStateSync.syncPersistentState(ecsWorld,
                                        playerEntity,
                                        persistentInventoryItems,
                                        persistentGoldAmount);

    if (ecsWorld.hasAny<CollectibleRunState>())
        persistentCollectibles = ecsWorld.getSingleton<CollectibleRunState>();
}
