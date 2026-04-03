#include "DungeonTestScene.h"

#include <limits>
#include <string>

#include "BitmapFontLoader.h"
#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "DebugCameraStateComponent.h"
#include "DungeonConstants.h"
#include "DungeonFloorSingleton.h"
#include "DungeonFreeFlyCamera.hpp"
#include "DungeonInputComponent.h"
#include "FloorTransitionStateComponent.h"
#include "GtsDebugOverlay.h"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "PlayerComponent.h"
#include "PlayerCameraSystem.h"
#include "PlayerMovementSystem.hpp"
#include "ProceduralMeshComponent.h"
#include "TransformComponent.h"
#include "UiTextDesc.h"
#include "UiTree.h"

#include "systems/DungeonInputSystem.hpp"

namespace
{
    constexpr float TEST_WALL_HEIGHT = 2.25f;

    glm::vec4 tintForTile(TileType tileType)
    {
        switch (tileType)
        {
            case TileType::StairUp:    return {0.20f, 0.80f, 0.25f, 1.0f};
            case TileType::StairDown:  return {0.85f, 0.20f, 0.20f, 1.0f};
            case TileType::Treasure:   return {0.95f, 0.82f, 0.18f, 1.0f};
            case TileType::EnemySpawn: return {0.62f, 0.30f, 0.75f, 1.0f};
            case TileType::Wall:       return {0.18f, 0.18f, 0.22f, 1.0f};
            case TileType::Floor:      return {0.60f, 0.60f, 0.60f, 1.0f};
        }

        return {1.0f, 1.0f, 1.0f, 1.0f};
    }

    glm::vec3 gridToWorld(const glm::ivec2& gridPos)
    {
        return {gridPos.x + 0.5f, 0.5f, gridPos.y + 0.5f};
    }

    bool isWall(const GeneratedFloor& floor, int x, int z)
    {
        return floor.inBounds(x, z) && floor.get(x, z) == TileType::Wall;
    }

    bool isWalkable(const GeneratedFloor& floor, int x, int z)
    {
        return floor.inBounds(x, z) && floor.isWalkable(x, z);
    }

    void spawnWallSegment(ECSWorld& world,
                          std::vector<Entity>& floorEntities,
                          float x,
                          float y,
                          float z,
                          float yawRadians)
    {
        Entity e = world.createEntity();
        floorEntities.push_back(e);

        ProceduralMeshComponent mesh;
        mesh.width  = 1.0f;
        mesh.height = 1.0f;

        MaterialComponent material;
        material.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
        material.tint        = tintForTile(TileType::Wall);
        material.doubleSided = false;

        TransformComponent transform;
        transform.position   = {x, y, z};
        transform.rotation.y = yawRadians;
        transform.scale      = {1.0f, TEST_WALL_HEIGHT, 1.0f};

        BoundsComponent bounds;
        bounds.min = {-0.5f, -0.5f * TEST_WALL_HEIGHT, -0.05f};
        bounds.max = { 0.5f,  0.5f * TEST_WALL_HEIGHT,  0.05f};

        world.addComponent(e, transform);
        world.addComponent(e, mesh);
        world.addComponent(e, material);
        world.addComponent(e, bounds);
    }
}

DungeonTestScene::DungeonTestScene()
    : dungeon(0x12345u, 4)
{
}

void DungeonTestScene::onLoad(SceneContext& ctx, const GtsSceneTransitionData* /*data*/)
{
    ecsWorld.clear();
    floorEntities.clear();
    playerEntity = INVALID_ENTITY;
    debugCameraEntity = INVALID_ENTITY;
    playerMarkerEntity = INVALID_ENTITY;
    overlayHandle = UI_INVALID_HANDLE;
    debugCameraActive = false;
    stairLatchActive = false;
    latchedFloorIndex = -1;
    latchedStairPos = {-1, -1};

    dungeon.generateRun();

    ecsWorld.createSingleton<DebugCameraStateComponent>();
    ecsWorld.createSingleton<DungeonInputComponent>();
    ecsWorld.createSingleton<FloorTransitionStateComponent>();

    initializeDungeonSingleton();
    buildFloorEntities(dungeon.getActiveFloor());
    spawnPlayer(ctx, dungeon.getActiveFloor().playerStart);
    spawnDebugCamera(ctx);
    spawnPlayerMarker();

    overlayFont = BitmapFontLoader::load(
        ctx.resources,
        GtsDebugOverlay::DEBUG_FONT_PATH,
        GtsDebugOverlay::ATLAS_W,  GtsDebugOverlay::ATLAS_H,
        GtsDebugOverlay::CELL_W,   GtsDebugOverlay::CELL_H,
        GtsDebugOverlay::ATLAS_COLS,
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ :/",
        GtsDebugOverlay::LINE_HEIGHT,
        true);

    overlayHandle = ctx.ui->addText({
        .text    = "FLOOR 1 / 4",
        .font    = &overlayFont,
        .x       = 0.02f,
        .y       = 0.02f,
        .scale   = GtsDebugOverlay::FONT_SCALE,
        .visible = true
    });

    ecsWorld.addControllerSystem<DungeonInputSystem>();
    ecsWorld.addControllerSystem<PlayerMovementSystem>();
    ecsWorld.addControllerSystem<PlayerCameraSystem>();
    ecsWorld.addControllerSystem<DungeonFreeFlyCamera>();

    installRendererFeature();
}

void DungeonTestScene::onUpdateSimulation(SceneContext& ctx)
{
    ecsWorld.updateSimulation(ctx.time->deltaTime);
}

void DungeonTestScene::onUpdateControllers(SceneContext& ctx)
{
    ecsWorld.updateControllers(ctx);

    handleDebugCameraToggle(ctx);
    handleDungeonRegenerate(ctx);
    handleStairTransitions(ctx);
    syncPlayerMarker();
    refreshOverlay(ctx);
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

void DungeonTestScene::rebuildActiveFloor(SceneContext& /*ctx*/)
{
    destroyFloorEntities();

    auto& floorSingleton = ecsWorld.getSingleton<DungeonFloorSingleton>();
    floorSingleton.floor = &dungeon.getActiveFloor();
    floorSingleton.currentFloorIndex = dungeon.getCurrentFloorIndex();

    const auto& floors = dungeon.getFloors();
    for (int i = 0; i < dungeon.getTotalFloorCount(); ++i)
        floorSingleton.allFloors[i] = &floors[static_cast<size_t>(i)];

    buildFloorEntities(dungeon.getActiveFloor());
}

void DungeonTestScene::buildFloorEntities(const GeneratedFloor& floor)
{
    const std::string floorTexture = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";

    for (int z = 0; z < floor.height; ++z)
    {
        for (int x = 0; x < floor.width; ++x)
        {
            const TileType tileType = floor.get(x, z);
            if (tileType != TileType::Wall)
            {
                Entity e = ecsWorld.createEntity();
                floorEntities.push_back(e);

                ProceduralMeshComponent mesh;
                MaterialComponent       material;
                TransformComponent      transform;
                BoundsComponent         bounds;

                material.texturePath = floorTexture;
                material.tint        = tintForTile(tileType);
                material.doubleSided = true;

                mesh.width        = 1.0f;
                mesh.height       = 1.0f;
                transform.position = {x + 0.5f, 0.0f, z + 0.5f};
                transform.rotation.x = -glm::half_pi<float>();
                bounds.min = {-0.5f, -0.5f, -0.02f};
                bounds.max = { 0.5f,  0.5f,  0.02f};
                ecsWorld.addComponent(e, transform);
                ecsWorld.addComponent(e, mesh);
                ecsWorld.addComponent(e, material);
                ecsWorld.addComponent(e, bounds);
            }
            else
            {
                if (isWalkable(floor, x, z - 1))
                    spawnWallSegment(ecsWorld, floorEntities,
                                     x + 0.5f, TEST_WALL_HEIGHT * 0.5f, static_cast<float>(z),
                                     glm::pi<float>());

                if (isWalkable(floor, x, z + 1))
                    spawnWallSegment(ecsWorld, floorEntities,
                                     x + 0.5f, TEST_WALL_HEIGHT * 0.5f, z + 1.0f,
                                     0.0f);

                if (isWalkable(floor, x + 1, z))
                    spawnWallSegment(ecsWorld, floorEntities,
                                     x + 1.0f, TEST_WALL_HEIGHT * 0.5f, z + 0.5f,
                                     -glm::half_pi<float>());

                if (isWalkable(floor, x - 1, z))
                    spawnWallSegment(ecsWorld, floorEntities,
                                     static_cast<float>(x), TEST_WALL_HEIGHT * 0.5f, z + 0.5f,
                                     glm::half_pi<float>());
            }
        }
    }
}

void DungeonTestScene::destroyFloorEntities()
{
    for (Entity entity : floorEntities)
        ecsWorld.destroyEntity(entity);

    floorEntities.clear();
}

void DungeonTestScene::spawnPlayer(SceneContext& ctx, const glm::ivec2& startPos)
{
    playerEntity = ecsWorld.createEntity();

    PlayerComponent player;
    player.gridX = startPos.x;
    player.gridZ = startPos.y;
    player.facing = 1;
    player.fromPosition = gridToWorld(startPos);
    player.toPosition   = gridToWorld(startPos);
    player.fromYaw = 90.0f;
    player.toYaw   = 90.0f;
    ecsWorld.addComponent(playerEntity, player);

    TransformComponent transform;
    transform.position = gridToWorld(startPos);
    ecsWorld.addComponent(playerEntity, transform);

    CameraDescriptionComponent camera;
    camera.active      = true;
    camera.fov         = glm::radians(70.0f);
    camera.aspectRatio = ctx.windowAspectRatio;
    camera.nearClip    = 0.05f;
    camera.farClip     = 100.0f;
    camera.target      = transform.position + glm::vec3(1.0f, 0.0f, 0.0f);
    ecsWorld.addComponent(playerEntity, camera);
}

void DungeonTestScene::spawnDebugCamera(SceneContext& ctx)
{
    debugCameraEntity = ecsWorld.createEntity();

    CameraDescriptionComponent camera;
    camera.active      = false;
    camera.fov         = glm::radians(70.0f);
    camera.aspectRatio = ctx.windowAspectRatio;
    camera.nearClip    = 0.05f;
    camera.farClip     = 200.0f;
    camera.target      = {0.0f, 0.0f, 0.0f};
    ecsWorld.addComponent(debugCameraEntity, camera);

    TransformComponent transform;
    transform.position = ecsWorld.getComponent<TransformComponent>(playerEntity).position + glm::vec3(0.0f, 5.0f, 0.0f);
    ecsWorld.addComponent(debugCameraEntity, transform);

    ecsWorld.addComponent(debugCameraEntity, CameraControlOverrideComponent{});
}

void DungeonTestScene::spawnPlayerMarker()
{
    playerMarkerEntity = ecsWorld.createEntity();

    ProceduralMeshComponent mesh;
    mesh.width  = 0.6f;
    mesh.height = 0.6f;
    ecsWorld.addComponent(playerMarkerEntity, mesh);

    MaterialComponent material;
    material.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/pictures/furina.jpg";
    material.doubleSided = true;
    ecsWorld.addComponent(playerMarkerEntity, material);

    TransformComponent transform;
    transform.rotation.x = -glm::half_pi<float>();
    transform.position   = {0.0f, 0.1f, 0.0f};
    ecsWorld.addComponent(playerMarkerEntity, transform);

    BoundsComponent bounds;
    bounds.min = {-0.3f, -0.3f, -0.02f};
    bounds.max = { 0.3f,  0.3f,  0.02f};
    ecsWorld.addComponent(playerMarkerEntity, bounds);

    syncPlayerMarker();
}

void DungeonTestScene::movePlayerToTile(const glm::ivec2& gridPos)
{
    auto& player    = ecsWorld.getComponent<PlayerComponent>(playerEntity);
    auto& transform = ecsWorld.getComponent<TransformComponent>(playerEntity);
    auto& camera    = ecsWorld.getComponent<CameraDescriptionComponent>(playerEntity);

    const glm::vec3 worldPos = gridToWorld(gridPos);

    player.gridX = gridPos.x;
    player.gridZ = gridPos.y;
    player.fromPosition = worldPos;
    player.toPosition   = worldPos;
    player.inTransition = false;
    player.transitionProgress = 1.0f;
    transform.position = worldPos;

    const float yawRad = glm::radians(player.toYaw);
    camera.target = worldPos + glm::vec3(glm::sin(yawRad), 0.0f, -glm::cos(yawRad));
}

void DungeonTestScene::syncPlayerMarker()
{
    if (playerMarkerEntity == INVALID_ENTITY || playerEntity == INVALID_ENTITY) return;

    const auto& player = ecsWorld.getComponent<PlayerComponent>(playerEntity);
    auto& markerTransform = ecsWorld.getComponent<TransformComponent>(playerMarkerEntity);
    markerTransform.position = {player.gridX + 0.5f, 0.1f, player.gridZ + 0.5f};
}

void DungeonTestScene::refreshOverlay(SceneContext& ctx)
{
    if (overlayHandle == UI_INVALID_HANDLE) return;

    ctx.ui->update(overlayHandle, UiTextDesc{
        .text    = "FLOOR: " + std::to_string(dungeon.getCurrentFloorIndex() + 1)
                 + " / " + std::to_string(dungeon.getTotalFloorCount())
                 + (debugCameraActive ? "  CAM: DEBUG" : "  CAM: PLAYER"),
        .font    = &overlayFont,
        .x       = 0.02f,
        .y       = 0.02f,
        .scale   = GtsDebugOverlay::FONT_SCALE,
        .visible = true
    });
}

void DungeonTestScene::handleDebugCameraToggle(SceneContext& ctx)
{
    const auto& input = ecsWorld.getSingleton<DungeonInputComponent>();
    if (!input.toggleDebugCamera) return;

    debugCameraActive = !debugCameraActive;
    ecsWorld.getSingleton<DebugCameraStateComponent>().active = debugCameraActive;

    auto& playerCamera = ecsWorld.getComponent<CameraDescriptionComponent>(playerEntity);
    auto& debugCamera  = ecsWorld.getComponent<CameraDescriptionComponent>(debugCameraEntity);

    if (debugCameraActive)
    {
        const glm::vec3 playerPos = ecsWorld.getComponent<TransformComponent>(playerEntity).position;
        auto& debugTransform = ecsWorld.getComponent<TransformComponent>(debugCameraEntity);
        debugTransform.position = playerPos + glm::vec3(0.0f, 5.0f, 0.0f);

        playerCamera.active = false;
        debugCamera.active  = true;
        debugCamera.aspectRatio = ctx.windowAspectRatio;
        debugCamera.target  = playerPos;
    }
    else
    {
        playerCamera.active = true;
        debugCamera.active  = false;
        playerCamera.aspectRatio = ctx.windowAspectRatio;
    }
}

void DungeonTestScene::handleDungeonRegenerate(SceneContext& ctx)
{
    const auto& input = ecsWorld.getSingleton<DungeonInputComponent>();
    if (!input.regeneratePressed) return;

    dungeon.generateRun();
    dungeon.setCurrentFloorIndex(0);
    rebuildActiveFloor(ctx);
    movePlayerToTile(dungeon.getActiveFloor().playerStart);
    if (debugCameraActive)
    {
        auto& debugTransform = ecsWorld.getComponent<TransformComponent>(debugCameraEntity);
        debugTransform.position = ecsWorld.getComponent<TransformComponent>(playerEntity).position
                                + glm::vec3(0.0f, 5.0f, 0.0f);
    }
    stairLatchActive = false;
    latchedFloorIndex = -1;
    latchedStairPos = {-1, -1};
}

void DungeonTestScene::handleStairTransitions(SceneContext& ctx)
{
    auto& player = ecsWorld.getComponent<PlayerComponent>(playerEntity);
    if (player.inTransition) return;

    const glm::ivec2 playerGridPos = {player.gridX, player.gridZ};
    const GeneratedFloor& floor = dungeon.getActiveFloor();

    updateStairLatch(floor, playerGridPos);
    if (stairLatchActive
        && latchedFloorIndex == dungeon.getCurrentFloorIndex()
        && latchedStairPos == playerGridPos)
        return;

    const TileType tileType = floor.get(player.gridX, player.gridZ);

    bool moved = false;
    glm::ivec2 destination = playerGridPos;

    if (tileType == TileType::StairDown && floor.hasStairDown())
    {
        moved = dungeon.moveDown();
        if (moved)
        {
            const GeneratedFloor& nextFloor = dungeon.getActiveFloor();
            destination = nextFloor.hasStairUp() ? nextFloor.stairUpPos : nextFloor.playerStart;
        }
    }
    else if (tileType == TileType::StairUp && floor.hasStairUp())
    {
        moved = dungeon.moveUp();
        if (moved)
        {
            const GeneratedFloor& nextFloor = dungeon.getActiveFloor();
            destination = nextFloor.hasStairDown() ? nextFloor.stairDownPos : nextFloor.playerStart;
        }
    }

    if (!moved) return;

    rebuildActiveFloor(ctx);
    movePlayerToTile(destination);
    if (debugCameraActive)
    {
        auto& debugTransform = ecsWorld.getComponent<TransformComponent>(debugCameraEntity);
        debugTransform.position = ecsWorld.getComponent<TransformComponent>(playerEntity).position
                                + glm::vec3(0.0f, 5.0f, 0.0f);
    }
    stairLatchActive = true;
    latchedFloorIndex = dungeon.getCurrentFloorIndex();
    latchedStairPos = destination;
}

void DungeonTestScene::updateStairLatch(const GeneratedFloor& /*floor*/, const glm::ivec2& playerGridPos)
{
    if (!stairLatchActive) return;

    if (latchedFloorIndex != dungeon.getCurrentFloorIndex() || latchedStairPos != playerGridPos)
    {
        stairLatchActive = false;
        latchedFloorIndex = -1;
        latchedStairPos = {-1, -1};
    }
}
