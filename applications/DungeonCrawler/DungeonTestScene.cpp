#include "DungeonTestScene.h"

#include <algorithm>
#include <limits>
#include <string>

#include "BitmapFontLoader.h"
#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "DebugCameraStateComponent.h"
#include "DungeonConstants.h"
#include "DungeonFloorSingleton.h"
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
#include "UiSystem.h"

#include "systems/DungeonInputSystem.hpp"

namespace
{
    constexpr float TEST_WALL_HEIGHT = 2.25f;
    constexpr float MINIMAP_CELL_SIZE = 0.0048f;
    constexpr float MINIMAP_PADDING = 0.010f;
    constexpr float MINIMAP_LABEL_HEIGHT = 0.030f;
    constexpr float MINIMAP_PLAYER_INSET = 0.0014f;

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

    UiColor uiColor(float r, float g, float b, float a = 1.0f)
    {
        return {r, g, b, a};
    }

    UiColor minimapColorForTile(TileType tileType)
    {
        switch (tileType)
        {
            case TileType::Wall:       return uiColor(0.12f, 0.13f, 0.15f);
            case TileType::Floor:      return uiColor(0.72f, 0.72f, 0.74f);
            case TileType::StairUp:    return uiColor(0.20f, 0.82f, 0.28f);
            case TileType::StairDown:  return uiColor(0.88f, 0.24f, 0.20f);
            case TileType::Treasure:   return uiColor(0.95f, 0.82f, 0.22f);
            case TileType::EnemySpawn: return uiColor(0.64f, 0.34f, 0.82f);
        }

        return uiColor(1.0f, 1.0f, 1.0f);
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
    overlayRootHandle = UI_INVALID_HANDLE;
    overlayBackgroundHandle = UI_INVALID_HANDLE;
    overlayTextHandle = UI_INVALID_HANDLE;
    minimapRootHandle = UI_INVALID_HANDLE;
    minimapBackgroundHandle = UI_INVALID_HANDLE;
    minimapGridHandle = UI_INVALID_HANDLE;
    minimapPlayerHandle = UI_INVALID_HANDLE;
    minimapLabelHandle = UI_INVALID_HANDLE;
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

    overlayFont = BitmapFontLoader::load(
        ctx.resources,
        GtsDebugOverlay::DEBUG_FONT_PATH,
        GtsDebugOverlay::ATLAS_W,  GtsDebugOverlay::ATLAS_H,
        GtsDebugOverlay::CELL_W,   GtsDebugOverlay::CELL_H,
        GtsDebugOverlay::ATLAS_COLS,
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ :/",
        GtsDebugOverlay::LINE_HEIGHT,
        true);

    overlayRootHandle = ctx.ui->createNode(UiNodeType::Container);
    overlayBackgroundHandle = ctx.ui->createNode(UiNodeType::Rect, overlayRootHandle);
    overlayTextHandle = ctx.ui->createNode(UiNodeType::Text, overlayRootHandle);

    UiLayoutSpec overlayRootLayout;
    overlayRootLayout.positionMode = UiPositionMode::Absolute;
    overlayRootLayout.widthMode    = UiSizeMode::Fixed;
    overlayRootLayout.heightMode   = UiSizeMode::Fixed;
    overlayRootLayout.offsetMin    = {0.015f, 0.015f};
    overlayRootLayout.fixedWidth   = 0.34f;
    overlayRootLayout.fixedHeight  = 0.07f;
    overlayRootLayout.padding      = {0.012f, 0.012f, 0.012f, 0.012f};
    ctx.ui->setLayout(overlayRootHandle, overlayRootLayout);
    ctx.ui->setState(overlayRootHandle, UiStateFlags{
        .visible = true,
        .enabled = false,
        .interactable = false
    });

    UiLayoutSpec backgroundLayout;
    backgroundLayout.positionMode = UiPositionMode::Anchored;
    backgroundLayout.widthMode    = UiSizeMode::FromAnchors;
    backgroundLayout.heightMode   = UiSizeMode::FromAnchors;
    backgroundLayout.anchorMin    = {0.0f, 0.0f};
    backgroundLayout.anchorMax    = {1.0f, 1.0f};
    ctx.ui->setLayout(overlayBackgroundHandle, backgroundLayout);
    ctx.ui->setState(overlayBackgroundHandle, UiStateFlags{
        .visible = true,
        .enabled = false,
        .interactable = false
    });
    ctx.ui->setPayload(overlayBackgroundHandle, UiRectData{
        {0.03f, 0.04f, 0.06f, 0.82f}
    });

    UiLayoutSpec textLayout;
    textLayout.positionMode = UiPositionMode::Absolute;
    textLayout.widthMode    = UiSizeMode::Fixed;
    textLayout.heightMode   = UiSizeMode::Fixed;
    textLayout.offsetMin    = {0.012f, 0.014f};
    ctx.ui->setLayout(overlayTextHandle, textLayout);
    ctx.ui->setState(overlayTextHandle, UiStateFlags{
        .visible = true,
        .enabled = false,
        .interactable = false
    });
    ctx.ui->setPayload(overlayTextHandle, UiTextData{
        "FLOOR: 1 / 4  CAM: PLAYER",
        {},
        {1.0f, 1.0f, 1.0f, 1.0f},
        GtsDebugOverlay::FONT_SCALE
    });
    ctx.ui->setTextFont(overlayTextHandle, &overlayFont);

    buildMinimapUi(ctx);
    refreshMinimap(ctx);

    ecsWorld.addControllerSystem<DungeonInputSystem>();
    ecsWorld.addControllerSystem<PlayerMovementSystem>();
    ecsWorld.addControllerSystem<PlayerCameraSystem>();

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
    refreshMinimap(ctx);
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
    if (overlayTextHandle == UI_INVALID_HANDLE) return;
    const bool debugCameraActive = ecsWorld.hasAny<DebugCameraStateComponent>()
        && ecsWorld.getSingleton<DebugCameraStateComponent>().active;
    const char* minimapMode = visibilityState.getRevealMode() == MinimapRevealMode::FullReveal
        ? "FULL"
        : "EXPLORED";

    ctx.ui->setPayload(overlayTextHandle, UiTextData{
        "FLOOR: " + std::to_string(dungeon.getCurrentFloorIndex() + 1)
            + " / " + std::to_string(dungeon.getTotalFloorCount())
            + (debugCameraActive ? "  CAM: DEBUG" : "  CAM: PLAYER")
            + "  MAP: " + minimapMode,
        {},
        {1.0f, 1.0f, 1.0f, 1.0f},
        GtsDebugOverlay::FONT_SCALE
    });
}

void DungeonTestScene::handleDungeonRegenerate(SceneContext& ctx)
{
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
    refreshMinimap(ctx);
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
    refreshMinimap(ctx);
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

void DungeonTestScene::buildMinimapUi(SceneContext& ctx)
{
    minimapRootHandle = ctx.ui->createNode(UiNodeType::Container);
    minimapBackgroundHandle = ctx.ui->createNode(UiNodeType::Rect, minimapRootHandle);
    minimapGridHandle = ctx.ui->createNode(UiNodeType::Grid, minimapRootHandle);
    minimapPlayerHandle = ctx.ui->createNode(UiNodeType::Rect, minimapRootHandle);
    minimapLabelHandle = ctx.ui->createNode(UiNodeType::Text, minimapRootHandle);

    UiLayoutSpec rootLayout;
    rootLayout.positionMode = UiPositionMode::Absolute;
    rootLayout.widthMode = UiSizeMode::Fixed;
    rootLayout.heightMode = UiSizeMode::Fixed;
    rootLayout.offsetMin = {0.74f, 0.04f};
    rootLayout.fixedWidth = 0.22f;
    rootLayout.fixedHeight = 0.22f;
    rootLayout.padding = {MINIMAP_PADDING, MINIMAP_PADDING, MINIMAP_PADDING, MINIMAP_PADDING};
    rootLayout.clipMode = UiClipMode::ClipChildren;
    ctx.ui->setLayout(minimapRootHandle, rootLayout);
    ctx.ui->setState(minimapRootHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});

    UiLayoutSpec backgroundLayout;
    backgroundLayout.positionMode = UiPositionMode::Anchored;
    backgroundLayout.widthMode = UiSizeMode::FromAnchors;
    backgroundLayout.heightMode = UiSizeMode::FromAnchors;
    backgroundLayout.anchorMin = {0.0f, 0.0f};
    backgroundLayout.anchorMax = {1.0f, 1.0f};
    ctx.ui->setLayout(minimapBackgroundHandle, backgroundLayout);
    ctx.ui->setState(minimapBackgroundHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
    ctx.ui->setPayload(minimapBackgroundHandle, UiRectData{uiColor(0.05f, 0.06f, 0.08f, 0.88f)});

    UiLayoutSpec gridLayout;
    gridLayout.positionMode = UiPositionMode::Absolute;
    gridLayout.widthMode = UiSizeMode::Fixed;
    gridLayout.heightMode = UiSizeMode::Fixed;
    gridLayout.offsetMin = {MINIMAP_PADDING, MINIMAP_PADDING + MINIMAP_LABEL_HEIGHT};
    ctx.ui->setLayout(minimapGridHandle, gridLayout);
    ctx.ui->setState(minimapGridHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});

    UiLayoutSpec playerLayout;
    playerLayout.positionMode = UiPositionMode::Absolute;
    playerLayout.widthMode = UiSizeMode::Fixed;
    playerLayout.heightMode = UiSizeMode::Fixed;
    ctx.ui->setLayout(minimapPlayerHandle, playerLayout);
    ctx.ui->setState(minimapPlayerHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
    ctx.ui->setPayload(minimapPlayerHandle, UiRectData{uiColor(0.14f, 0.86f, 1.0f, 1.0f)});

    UiLayoutSpec labelLayout;
    labelLayout.positionMode = UiPositionMode::Absolute;
    labelLayout.widthMode = UiSizeMode::Fixed;
    labelLayout.heightMode = UiSizeMode::Fixed;
    labelLayout.offsetMin = {MINIMAP_PADDING, 0.006f};
    ctx.ui->setLayout(minimapLabelHandle, labelLayout);
    ctx.ui->setState(minimapLabelHandle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
    ctx.ui->setPayload(minimapLabelHandle, UiTextData{
        "MAP F1",
        {},
        uiColor(0.95f, 0.96f, 1.0f, 1.0f),
        0.020f
    });
    ctx.ui->setTextFont(minimapLabelHandle, &overlayFont);
}

void DungeonTestScene::refreshMinimap(SceneContext& ctx)
{
    if (minimapRootHandle == UI_INVALID_HANDLE || playerEntity == INVALID_ENTITY) return;

    const GeneratedFloor& floor = dungeon.getActiveFloor();
    const PlayerComponent& player = ecsWorld.getComponent<PlayerComponent>(playerEntity);

    const float gridWidth = floor.width * MINIMAP_CELL_SIZE;
    const float gridHeight = floor.height * MINIMAP_CELL_SIZE;
    const float containerWidth = gridWidth + MINIMAP_PADDING * 2.0f;
    const float containerHeight = gridHeight + MINIMAP_PADDING * 2.0f + MINIMAP_LABEL_HEIGHT;

    UiLayoutSpec rootLayout;
    rootLayout.positionMode = UiPositionMode::Absolute;
    rootLayout.widthMode = UiSizeMode::Fixed;
    rootLayout.heightMode = UiSizeMode::Fixed;
    rootLayout.offsetMin = {std::max(0.02f, 0.98f - containerWidth), 0.04f};
    rootLayout.fixedWidth = containerWidth;
    rootLayout.fixedHeight = containerHeight;
    rootLayout.padding = {MINIMAP_PADDING, MINIMAP_PADDING, MINIMAP_PADDING, MINIMAP_PADDING};
    rootLayout.clipMode = UiClipMode::ClipChildren;
    ctx.ui->setLayout(minimapRootHandle, rootLayout);

    UiLayoutSpec gridLayout;
    gridLayout.positionMode = UiPositionMode::Absolute;
    gridLayout.widthMode = UiSizeMode::Fixed;
    gridLayout.heightMode = UiSizeMode::Fixed;
    gridLayout.offsetMin = {MINIMAP_PADDING, MINIMAP_PADDING + MINIMAP_LABEL_HEIGHT};
    gridLayout.fixedWidth = gridWidth;
    gridLayout.fixedHeight = gridHeight;
    ctx.ui->setLayout(minimapGridHandle, gridLayout);

    UiGridData gridData;
    gridData.columns = floor.width;
    gridData.rows = floor.height;
    gridData.hiddenColor = uiColor(0.0f, 0.0f, 0.0f, 1.0f);
    gridData.cellInset = 0.00025f;
    gridData.cells.reserve(static_cast<size_t>(floor.width * floor.height));

    for (int z = 0; z < floor.height; ++z)
    {
        for (int x = 0; x < floor.width; ++x)
        {
            gridData.cells.push_back(UiGridCellData{
                minimapColorForTile(floor.get(x, z)),
                visibilityState.isVisible(dungeon.getCurrentFloorIndex(), x, z)
            });
        }
    }

    ctx.ui->setPayload(minimapGridHandle, gridData);

    UiLayoutSpec playerLayout;
    playerLayout.positionMode = UiPositionMode::Absolute;
    playerLayout.widthMode = UiSizeMode::Fixed;
    playerLayout.heightMode = UiSizeMode::Fixed;
    playerLayout.offsetMin = {
        MINIMAP_PADDING + player.gridX * MINIMAP_CELL_SIZE + MINIMAP_PLAYER_INSET,
        MINIMAP_PADDING + MINIMAP_LABEL_HEIGHT + player.gridZ * MINIMAP_CELL_SIZE + MINIMAP_PLAYER_INSET
    };
    playerLayout.fixedWidth = std::max(0.0f, MINIMAP_CELL_SIZE - MINIMAP_PLAYER_INSET * 2.0f);
    playerLayout.fixedHeight = std::max(0.0f, MINIMAP_CELL_SIZE - MINIMAP_PLAYER_INSET * 2.0f);
    ctx.ui->setLayout(minimapPlayerHandle, playerLayout);
    ctx.ui->setState(minimapPlayerHandle, UiStateFlags{
        .visible = visibilityState.isVisible(dungeon.getCurrentFloorIndex(), player.gridX, player.gridZ),
        .enabled = false,
        .interactable = false
    });

    const char* revealModeText = visibilityState.getRevealMode() == MinimapRevealMode::FullReveal
        ? "FULL"
        : "FOG";
    ctx.ui->setPayload(minimapLabelHandle, UiTextData{
        "MAP F" + std::to_string(dungeon.getCurrentFloorIndex() + 1) + " " + revealModeText,
        {},
        uiColor(0.95f, 0.96f, 1.0f, 1.0f),
        0.020f
    });
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
