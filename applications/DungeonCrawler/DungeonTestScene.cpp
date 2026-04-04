#include "DungeonTestScene.h"

#include <limits>
#include <string>

#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "DebugCameraStateComponent.h"
#include "DungeonConstants.h"
#include "DungeonFloorSingleton.h"
#include "DungeonInputComponent.h"
#include "EnemyComponent.h"
#include "EnemyMovementStateComponent.h"
#include "EnemyTagComponent.h"
#include "FloorTransitionStateComponent.h"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "PlayerComponent.h"
#include "PlayerCameraSystem.h"
#include "PlayerMovementSystem.hpp"
#include "ProceduralMeshComponent.h"
#include "RenderGpuComponent.h"
#include "RenderResourceClearComponent.h"
#include "RenderResourceClearSystem.hpp"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"

#include "systems/DungeonInputSystem.hpp"
#include "systems/EnemyMovementSystem.hpp"

namespace
{
    constexpr float TEST_WALL_HEIGHT = 2.25f;
    constexpr float STAIR_STEP_DROP = 0.22f;
    constexpr float STAIR_STEP_INSET = 0.14f;
    constexpr float STAIR_TRANSITION_DROP = 1.4f;
    constexpr float STAIR_TRANSITION_FORWARD = 0.35f;

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

    std::string textureForTile(TileType tileType)
    {
        const std::string base = GraphicsConstants::ENGINE_RESOURCES + "/textures/";

        switch (tileType)
        {
            case TileType::StairUp:
                return base + "green_texture.png";
            case TileType::StairDown:
                return base + "orange_texture.png";
            default:
                return base + "grey_texture.png";
        }
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

    glm::ivec2 findWalkableNeighborDirection(const GeneratedFloor& floor, const glm::ivec2& tilePos)
    {
        static constexpr glm::ivec2 directions[] = {
            {0, -1},
            {1, 0},
            {0, 1},
            {-1, 0},
        };

        for (const glm::ivec2& dir : directions)
        {
            const glm::ivec2 candidate = tilePos + dir;
            if (floor.isWalkable(candidate.x, candidate.y))
                return dir;
        }

        return {0, -1};
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
    playerMarkerEntity = INVALID_ENTITY;
    uiController.reset();
    stairLatchActive = false;
    latchedFloorIndex = -1;
    latchedStairPos = {-1, -1};

    dungeon.generateRun();
    visibilityState.initialize(dungeon.getFloors());
    visibilityState.revealAt(0,
                             dungeon.getActiveFloor(),
                             dungeon.getActiveFloor().playerStart.x,
                             dungeon.getActiveFloor().playerStart.y);

    ecsWorld.createSingleton<DungeonInputComponent>();
    ecsWorld.createSingleton<FloorTransitionStateComponent>();

    initializeDungeonSingleton();
    buildFloorEntities(dungeon.getActiveFloor());
    spawnPlayer(ctx, dungeon.getActiveFloor().playerStart);
    spawnPlayerMarker();
    uiController.initialize(ctx, buildUiState());

    ecsWorld.addControllerSystem<DungeonInputSystem>();
    ecsWorld.addControllerSystem<PlayerMovementSystem>();
    ecsWorld.addControllerSystem<PlayerCameraSystem>();
    ecsWorld.addSimulationSystem<EnemyMovementSystem>();

    installRendererFeature();
}

void DungeonTestScene::onUpdateSimulation(SceneContext& ctx)
{
    ecsWorld.updateSimulation(ctx.time->deltaTime);
}

void DungeonTestScene::onUpdateControllers(SceneContext& ctx)
{
    ecsWorld.updateControllers(ctx);

    handleDungeonRegenerate(ctx);
    handleStairTransitions(ctx);
    updateMinimapReveal(ctx);
    syncPlayerMarker();
    uiController.update(ctx, buildUiState());
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

void DungeonTestScene::rebuildActiveFloor(SceneContext& ctx)
{
    destroyFloorEntities(ctx);

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

                material.texturePath = textureForTile(tileType);
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

                if (tileType == TileType::StairDown || tileType == TileType::StairUp)
                    spawnStairFeature(floor, {x, z}, tileType == TileType::StairDown);
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

    spawnEnemyEntities(floor);
}

void DungeonTestScene::spawnEnemyEntities(const GeneratedFloor& floor)
{
    const std::string cubeMesh = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
    const std::string blueTexture = GraphicsConstants::ENGINE_RESOURCES + "/textures/blue_texture.png";

    for (size_t spawnIndex = 0; spawnIndex < floor.enemySpawnPositions.size(); ++spawnIndex)
    {
        const glm::vec3& spawnPosition = floor.enemySpawnPositions[spawnIndex];
        Entity e = ecsWorld.createEntity();
        floorEntities.push_back(e);

        TransformComponent transform;
        transform.position = spawnPosition;
        transform.scale    = {0.5f, 0.5f, 0.5f};

        EnemyComponent enemy;
        enemy.gridX      = static_cast<int>(spawnPosition.x);
        enemy.gridZ      = static_cast<int>(spawnPosition.z);
        enemy.floorIndex = floor.floorNumber;
        enemy.moveSpeed  = 2.0f + static_cast<float>((floor.floorNumber * 13 + static_cast<int>(spawnIndex) * 17) % 11) * 0.1f;
        enemy.rngState   = static_cast<uint32_t>((floor.floorNumber + 1) * 73856093
                       ^ (enemy.gridX + 1) * 19349663
                       ^ (enemy.gridZ + 1) * 83492791
                       ^ static_cast<int>(spawnIndex + 1) * 2654435761u);

        EnemyMovementStateComponent movement;
        movement.startPosition  = spawnPosition;
        movement.targetPosition = spawnPosition;
        movement.targetTile     = {enemy.gridX, enemy.gridZ};

        StaticMeshComponent mesh;
        mesh.meshPath = cubeMesh;

        MaterialComponent material;
        material.texturePath = blueTexture;
        material.tint        = {0.25f, 0.45f, 1.0f, 1.0f};

        BoundsComponent bounds;
        bounds.min = {-0.25f, -0.25f, -0.25f};
        bounds.max = { 0.25f,  0.25f,  0.25f};

        ecsWorld.addComponent(e, enemy);
        ecsWorld.addComponent(e, movement);
        ecsWorld.addComponent(e, EnemyTagComponent{});
        ecsWorld.addComponent(e, transform);
        ecsWorld.addComponent(e, mesh);
        ecsWorld.addComponent(e, material);
        ecsWorld.addComponent(e, bounds);
    }
}

void DungeonTestScene::spawnStairFeature(const GeneratedFloor& floor,
                                         const glm::ivec2& stairPos,
                                         bool descends)
{
    const TileType stairTile = descends ? TileType::StairDown : TileType::StairUp;
    const glm::ivec2 direction = findWalkableNeighborDirection(floor, stairPos);
    const glm::vec2 dirVec = glm::normalize(glm::vec2(static_cast<float>(direction.x),
                                                      static_cast<float>(direction.y)));

    for (int step = 0; step < 3; ++step)
    {
        Entity e = ecsWorld.createEntity();
        floorEntities.push_back(e);

        const float stepFactor = static_cast<float>(step + 1) / 3.0f;
        const float width = 1.0f - STAIR_STEP_INSET * 2.0f * stepFactor;
        const float depth = 1.0f - STAIR_STEP_INSET * 1.5f * stepFactor;
        const float yOffset = descends
            ? -STAIR_STEP_DROP * stepFactor
            : STAIR_STEP_DROP * (1.0f - stepFactor) * 0.75f;
        const glm::vec2 slide = dirVec * (0.12f * (stepFactor - 0.5f));

        ProceduralMeshComponent mesh;
        mesh.width = width;
        mesh.height = depth;

        MaterialComponent material;
        material.texturePath = textureForTile(stairTile);
        material.tint = tintForTile(stairTile);
        material.doubleSided = true;

        TransformComponent transform;
        transform.position = {
            stairPos.x + 0.5f + slide.x,
            yOffset,
            stairPos.y + 0.5f + slide.y
        };
        transform.rotation.x = -glm::half_pi<float>();

        BoundsComponent bounds;
        bounds.min = {-width * 0.5f, -depth * 0.5f, -0.02f};
        bounds.max = { width * 0.5f,  depth * 0.5f,  0.02f};

        ecsWorld.addComponent(e, transform);
        ecsWorld.addComponent(e, mesh);
        ecsWorld.addComponent(e, material);
        ecsWorld.addComponent(e, bounds);
    }
}

void DungeonTestScene::destroyFloorEntities(SceneContext& ctx)
{
    for (Entity entity : floorEntities)
    {
        if (ecsWorld.hasComponent<RenderGpuComponent>(entity)
            && !ecsWorld.hasComponent<RenderResourceClearComponent>(entity))
        {
            ecsWorld.addComponent(entity, RenderResourceClearComponent{});
        }
        else if (!ecsWorld.hasComponent<RenderGpuComponent>(entity))
        {
            ecsWorld.destroyEntity(entity);
        }
    }

    // Reclaim object SSBO slots before spawning the replacement floor.
    RenderResourceClearSystem{}.update(ecsWorld, ctx);

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

void DungeonTestScene::handleDungeonRegenerate(SceneContext& ctx)
{
    if (ecsWorld.getSingleton<FloorTransitionStateComponent>().active) return;

    const auto& input = ecsWorld.getSingleton<DungeonInputComponent>();
    if (!input.regeneratePressed) return;

    dungeon.generateRun();
    visibilityState.initialize(dungeon.getFloors());
    dungeon.setCurrentFloorIndex(0);
    visibilityState.revealAt(0,
                             dungeon.getActiveFloor(),
                             dungeon.getActiveFloor().playerStart.x,
                             dungeon.getActiveFloor().playerStart.y);
    rebuildActiveFloor(ctx);
    movePlayerToTile(dungeon.getActiveFloor().playerStart);
    stairLatchActive = false;
    latchedFloorIndex = -1;
    latchedStairPos = {-1, -1};
}

void DungeonTestScene::handleStairTransitions(SceneContext& ctx)
{
    auto& transition = ecsWorld.getSingleton<FloorTransitionStateComponent>();
    if (transition.active)
    {
        updateFloorTransition(ctx);
        return;
    }

    if (ecsWorld.hasAny<DebugCameraStateComponent>()
        && ecsWorld.getSingleton<DebugCameraStateComponent>().active)
        return;

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

    if (tileType == TileType::StairDown && floor.hasStairDown())
        beginFloorTransition(ctx, true, playerGridPos);
    else if (tileType == TileType::StairUp && floor.hasStairUp())
        beginFloorTransition(ctx, false, playerGridPos);
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

void DungeonTestScene::updateMinimapReveal(SceneContext& /*ctx*/)
{
    if (playerEntity == INVALID_ENTITY) return;

    const auto& input = ecsWorld.getSingleton<DungeonInputComponent>();
    if (input.toggleMinimapRevealPressed)
        visibilityState.toggleRevealMode();

    const PlayerComponent& player = ecsWorld.getComponent<PlayerComponent>(playerEntity);
    visibilityState.revealAt(dungeon.getCurrentFloorIndex(),
                             dungeon.getActiveFloor(),
                             player.gridX,
                             player.gridZ);
}

void DungeonTestScene::beginFloorTransition(SceneContext& /*ctx*/, bool movingDown, const glm::ivec2& sourceGridPos)
{
    const int sourceFloor = dungeon.getCurrentFloorIndex();
    const int targetFloor = movingDown ? sourceFloor + 1 : sourceFloor - 1;
    const GeneratedFloor* destinationFloor = dungeon.getFloor(targetFloor);
    if (!destinationFloor) return;

    glm::ivec2 arrival = destinationFloor->playerStart;
    if (movingDown && destinationFloor->hasStairUp())
        arrival = destinationFloor->stairUpPos;
    else if (!movingDown && destinationFloor->hasStairDown())
        arrival = destinationFloor->stairDownPos;

    auto& player = ecsWorld.getComponent<PlayerComponent>(playerEntity);
    auto& camera = ecsWorld.getComponent<CameraDescriptionComponent>(playerEntity);
    const glm::vec3 currentPos = ecsWorld.getComponent<TransformComponent>(playerEntity).position;

    glm::vec3 forward = camera.target - currentPos;
    if (glm::dot(forward, forward) < 0.0001f)
        forward = {1.0f, 0.0f, 0.0f};
    else
        forward = glm::normalize(forward);

    const glm::vec3 sourceWorld = gridToWorld(sourceGridPos);
    const glm::vec3 arrivalWorld = gridToWorld(arrival);
    const float dropOffset = movingDown ? -STAIR_TRANSITION_DROP : STAIR_TRANSITION_DROP;
    const glm::vec3 midOffset = forward * STAIR_TRANSITION_FORWARD + glm::vec3(0.0f, dropOffset, 0.0f);

    auto& transition = ecsWorld.getSingleton<FloorTransitionStateComponent>();
    transition.active = true;
    transition.type = FloorTransitionType::Stairs;
    transition.phase = movingDown ? FloorTransitionPhase::Descending : FloorTransitionPhase::Ascending;
    transition.progress = 0.0f;
    transition.targetFloor = targetFloor;
    transition.arrivalGrid = arrival;
    transition.floorSwapApplied = false;
    transition.camStart = currentPos;
    transition.camMid = sourceWorld + midOffset;
    transition.camEnd = arrivalWorld;
    transition.lookAtStart = currentPos + forward;
    transition.lookAtMid = sourceWorld + forward * 0.75f + glm::vec3(0.0f, dropOffset, 0.0f);
    transition.lookAtEnd = arrivalWorld + forward;

    player.inTransition = false;
}

void DungeonTestScene::updateFloorTransition(SceneContext& ctx)
{
    auto& transition = ecsWorld.getSingleton<FloorTransitionStateComponent>();
    if (!transition.active) return;

    if (ecsWorld.hasAny<DebugCameraStateComponent>())
    {
        auto& debugState = ecsWorld.getSingleton<DebugCameraStateComponent>();
        if (debugState.active)
        {
            debugState.active = false;

            if (ecsWorld.hasComponent<CameraDescriptionComponent>(debugState.debugCameraEntity))
                ecsWorld.getComponent<CameraDescriptionComponent>(debugState.debugCameraEntity).active = false;

            if (ecsWorld.hasComponent<CameraDescriptionComponent>(playerEntity))
                ecsWorld.getComponent<CameraDescriptionComponent>(playerEntity).active = true;
        }
    }

    transition.progress += ctx.time->unscaledDeltaTime / transition.getDuration();
    transition.progress = glm::clamp(transition.progress, 0.0f, 1.0f);

    const bool inFirstSegment = transition.progress < 0.5f;
    const float segmentT = inFirstSegment
        ? transition.progress * 2.0f
        : (transition.progress - 0.5f) * 2.0f;
    const float ease = segmentT * segmentT * (3.0f - 2.0f * segmentT);

    if (!transition.floorSwapApplied && transition.progress >= 0.5f)
    {
        if (dungeon.moveToFloor(transition.targetFloor))
        {
            rebuildActiveFloor(ctx);
            movePlayerToTile(transition.arrivalGrid);
            visibilityState.revealAt(dungeon.getCurrentFloorIndex(),
                                     dungeon.getActiveFloor(),
                                     transition.arrivalGrid.x,
                                     transition.arrivalGrid.y);
            stairLatchActive = true;
            latchedFloorIndex = dungeon.getCurrentFloorIndex();
            latchedStairPos = transition.arrivalGrid;
        }

        transition.floorSwapApplied = true;
        transition.phase = FloorTransitionPhase::Completing;
    }

    const glm::vec3 camPos = (!transition.floorSwapApplied)
        ? glm::mix(transition.camStart, transition.camMid, ease)
        : glm::mix(transition.camMid, transition.camEnd, ease);
    const glm::vec3 lookAt = (!transition.floorSwapApplied)
        ? glm::mix(transition.lookAtStart, transition.lookAtMid, ease)
        : glm::mix(transition.lookAtMid, transition.lookAtEnd, ease);

    auto& cameraTransform = ecsWorld.getComponent<TransformComponent>(playerEntity);
    auto& cameraDesc = ecsWorld.getComponent<CameraDescriptionComponent>(playerEntity);
    cameraTransform.position = camPos;
    cameraDesc.aspectRatio = ctx.windowAspectRatio;
    cameraDesc.target = lookAt;

    if (transition.progress >= 1.0f)
    {
        auto& player = ecsWorld.getComponent<PlayerComponent>(playerEntity);
        const float yawRad = glm::radians(player.toYaw);
        const glm::vec3 playerWorld = gridToWorld({player.gridX, player.gridZ});

        cameraTransform.position = playerWorld;
        cameraDesc.target = playerWorld + glm::vec3(glm::sin(yawRad), 0.0f, -glm::cos(yawRad));

        transition.active = false;
        transition.phase = FloorTransitionPhase::Idle;
        transition.progress = 0.0f;
        transition.floorSwapApplied = false;
    }
}

DungeonUiState DungeonTestScene::buildUiState()
{
    DungeonUiState state;
    state.currentFloorIndex = dungeon.getCurrentFloorIndex();
    state.totalFloorCount = dungeon.getTotalFloorCount();
    state.minimapRevealMode = visibilityState.getRevealMode();
    state.activeFloor = &dungeon.getActiveFloor();
    state.visibility = &visibilityState;

    if (ecsWorld.hasAny<DebugCameraStateComponent>())
        state.debugCameraActive = ecsWorld.getSingleton<DebugCameraStateComponent>().active;

    if (playerEntity != INVALID_ENTITY)
    {
        const PlayerComponent& player = ecsWorld.getComponent<PlayerComponent>(playerEntity);
        state.playerTile = {player.gridX, player.gridZ};
    }

    return state;
}
