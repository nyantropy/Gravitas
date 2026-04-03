#include "DungeonFloorScene.h"

#include <cmath>
#include <string>
#include <limits>
#include <vector>

// Engine components
#include "HierarchyComponent.h"
#include "TransformComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraResourceClearComponent.h"
#include "CameraResourceClearSystem.hpp"
#include "CameraControlOverrideComponent.h"
#include "RenderGpuComponent.h"
#include "RenderResourceClearComponent.h"
#include "RenderResourceClearSystem.hpp"
#include "BoundsComponent.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshComponent.h"
#include "MaterialComponent.h"
#include "GraphicsConstants.h"
#include "GlmConfig.h"

// Dungeon components
#include "DungeonFloorSingleton.h"
#include "DungeonInputComponent.h"
#include "DungeonGameStateComponent.h"
#include "PlayerComponent.h"
#include "DungeonTileComponent.h"
#include "FloorEntityTag.h"
#include "FloorEnemyComponent.h"
#include "FloorTransitionStateComponent.h"
#include "FloorTransitionTriggerComponent.h"
#include "DungeonConstants.h"

// Font + UI
#include "BitmapFontLoader.h"
#include "GtsDebugOverlay.h"
#include "UiTree.h"
#include "UiTextDesc.h"

// Dungeon systems
#include "DungeonInputSystem.hpp"
#include "PlayerMovementSystem.hpp"
#include "PlayerCameraSystem.h"
#include "FloorTransitionSystem.hpp"
#include "DungeonFreeFlyCamera.hpp"
#include "DungeonTileBindingSystem.hpp"
#include "EnemyMovementSystem.hpp"

// ─── helpers ─────────────────────────────────────────────────────────────────

// Returns the first walkable neighbour of stairPos on dest, or dest.playerStart
// if all four neighbours are walls. Used to place FloorTransitionTriggerComponent
// arrivalGrid one step past the arrival stair so the player does not re-trigger.
static glm::ivec2 findWalkableNeighbor(const GeneratedFloor& dest, glm::ivec2 stairPos)
{
    const glm::ivec2 dirs[4] = {{1,0},{0,1},{-1,0},{0,-1}};
    for (const auto& d : dirs)
    {
        const glm::ivec2 cand = {stairPos.x + d.x, stairPos.y + d.y};
        if (dest.isWalkable(cand.x, cand.y))
            return cand;
    }
    return dest.playerStart;
}

// ─── Constructor ─────────────────────────────────────────────────────────────
DungeonFloorScene::DungeonFloorScene()
    : dungeon(0x12345u, 4)
{
    dungeon.generateRun();
}

// ─── onLoad ──────────────────────────────────────────────────────────────────
void DungeonFloorScene::onLoad(SceneContext& ctx,
                                const GtsSceneTransitionData* /*data*/)
{
    // On reload, release GPU resources via the clear systems before wiping the world.
    ecsWorld.forEach<CameraDescriptionComponent>([&](Entity e, CameraDescriptionComponent&)
    {
        if (!ecsWorld.hasComponent<CameraResourceClearComponent>(e))
        {
            CameraResourceClearComponent c;
            c.destroyAfterClear = false;
            ecsWorld.addComponent(e, c);
        }
    });

    ecsWorld.forEach<RenderGpuComponent>([&](Entity e, RenderGpuComponent& rc)
    {
        if (rc.objectSSBOSlot != RENDERABLE_SLOT_UNALLOCATED &&
            !ecsWorld.hasComponent<RenderResourceClearComponent>(e))
        {
            RenderResourceClearComponent c;
            c.destroyAfterClear = false;
            ecsWorld.addComponent(e, c);
        }
    });

    CameraResourceClearSystem{}.update(ecsWorld, ctx);
    RenderResourceClearSystem{}.update(ecsWorld, ctx);

    ecsWorld.clear();
    playerEntity      = INVALID_ENTITY;
    debugCamEntity    = INVALID_ENTITY;
    lastDisplayedFloor = 0;

    // Singletons shared across dungeon systems
    ecsWorld.createSingleton<DungeonInputComponent>();
    ecsWorld.createSingleton<DungeonGameStateComponent>();
    ecsWorld.createSingleton<FloorTransitionStateComponent>();

    // Build DungeonFloorSingleton with all four floors
    auto& floorSingleton = ecsWorld.createSingleton<DungeonFloorSingleton>();
    const auto& floors = dungeon.getFloors();
    floorSingleton.run               = &dungeon;
    for (int i = 0; i < dungeon.getTotalFloorCount(); ++i)
        floorSingleton.allFloors[i] = &floors[static_cast<size_t>(i)];
    floorSingleton.floor             = &dungeon.getActiveFloor();
    floorSingleton.currentFloorIndex = dungeon.getCurrentFloorIndex();
    for (int i = 0; i < dungeon.getTotalFloorCount(); ++i)
        floorSingleton.floorWorldOffset[i] = {0.0f, DungeonConstants::floorWorldY(i), 0.0f};

    // Spawn all four floors at their Y offsets
    for (const GeneratedFloor& floor : floors)
    {
        spawnFloorTiles(ctx, floor);
        spawnEnemies(ctx, floor);
        spawnRamps(ctx, floor);
    }

    // Spawn player on floor 0
    spawnPlayer(ctx, dungeon.getActiveFloor().playerStart);
    spawnDebugCamera(ctx);

    // Floor indicator
    floorFont = BitmapFontLoader::load(
        ctx.resources,
        GtsDebugOverlay::DEBUG_FONT_PATH,
        GtsDebugOverlay::ATLAS_W,  GtsDebugOverlay::ATLAS_H,
        GtsDebugOverlay::CELL_W,   GtsDebugOverlay::CELL_H,
        GtsDebugOverlay::ATLAS_COLS,
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ ",
        GtsDebugOverlay::LINE_HEIGHT,
        true);

    floorIndicatorHandle = ctx.ui->addText({
        .text    = "FLOOR " + std::to_string(dungeon.getActiveFloor().floorNumber + 1),
        .font    = &floorFont,
        .x       = 0.02f,
        .y       = 0.02f,
        .scale   = GtsDebugOverlay::FONT_SCALE,
        .visible = true
    });

    // Systems — order: input → movement → camera → transition → debug cam → tile binding → enemy
    ecsWorld.addControllerSystem<DungeonInputSystem>();
    ecsWorld.addControllerSystem<PlayerMovementSystem>();
    ecsWorld.addControllerSystem<PlayerCameraSystem>();
    ecsWorld.addControllerSystem<FloorTransitionSystem>();
    ecsWorld.addControllerSystem<DungeonFreeFlyCamera>();
    ecsWorld.addControllerSystem<DungeonTileBindingSystem>();
    ecsWorld.addControllerSystem<EnemyMovementSystem>();

    // installRendererFeature must be LAST
    installRendererFeature();
}

// ─── onUpdateSimulation ──────────────────────────────────────────────────────
void DungeonFloorScene::onUpdateSimulation(SceneContext& ctx)
{
    ecsWorld.updateSimulation(ctx.time->deltaTime);
}

// ─── onUpdateControllers ─────────────────────────────────────────────────────
void DungeonFloorScene::onUpdateControllers(SceneContext& ctx)
{
    ecsWorld.updateControllers(ctx);

    // T — toggle between first-person and debug bird's-eye camera
    // (blocked during floor transitions so we don't interfere)
    const auto& input = ecsWorld.getSingleton<DungeonInputComponent>();
    const auto& ts    = ecsWorld.getSingleton<FloorTransitionStateComponent>();

    if (input.toggleDebugCamera && !ts.active)
    {
        auto& playerCam = ecsWorld.getComponent<CameraDescriptionComponent>(playerEntity);
        auto& dbgCam    = ecsWorld.getComponent<CameraDescriptionComponent>(debugCamEntity);
        const bool wasPlayerActive = playerCam.active;
        playerCam.active = !wasPlayerActive;
        dbgCam.active    =  wasPlayerActive;
    }

    // Update floor indicator UI when the player changes floors
    const auto& floorSingleton = ecsWorld.getSingleton<DungeonFloorSingleton>();
    if (floorSingleton.currentFloorIndex != lastDisplayedFloor)
    {
        lastDisplayedFloor = floorSingleton.currentFloorIndex;
        ctx.ui->update(floorIndicatorHandle, UiTextDesc{
            .text    = "FLOOR " + std::to_string(lastDisplayedFloor + 1),
            .font    = &floorFont,
            .x       = 0.02f,
            .y       = 0.02f,
            .scale   = GtsDebugOverlay::FONT_SCALE,
            .visible = true
        });
    }
}

// ─── spawnFloorTiles ─────────────────────────────────────────────────────────
void DungeonFloorScene::spawnFloorTiles(SceneContext& /*ctx*/, const GeneratedFloor& floor)
{
    const float floorY = DungeonConstants::floorWorldY(floor.floorNumber);

    for (int z = 0; z < floor.height; ++z)
    {
        for (int x = 0; x < floor.width; ++x)
        {
            const TileType t = floor.get(x, z);

            if (t == TileType::Wall) continue;

            Entity e = ecsWorld.createEntity();

            // All floor-geometry entities get FloorEntityTag
            ecsWorld.addComponent(e, FloorEntityTag{});

            DungeonTileComponent tile;
            tile.tileType    = t;
            tile.floorNumber = floor.floorNumber;
            ecsWorld.addComponent(e, tile);

            TransformComponent tc;
            tc.position   = {x + 0.5f, floorY, z + 0.5f};
            tc.rotation.x = -glm::half_pi<float>();
            tc.scale      = {1.0f, 1.0f, 1.0f};
            ecsWorld.addComponent(e, tc);

            BoundsComponent bc;
            bc.min = {-0.5f, -0.5f, -0.02f};
            bc.max = { 0.5f,  0.5f,  0.02f};
            ecsWorld.addComponent(e, bc);

            // Ceiling
            if (enableCeiling)
            {
                Entity ceiling = ecsWorld.createEntity();
                ecsWorld.addComponent(ceiling, FloorEntityTag{});

                ProceduralMeshComponent ceilMesh;
                ceilMesh.width  = 1.0f;
                ceilMesh.height = 1.0f;
                ecsWorld.addComponent(ceiling, ceilMesh);

                MaterialComponent ceilMat;
                ceilMat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
                ceilMat.doubleSided = true;
                ecsWorld.addComponent(ceiling, ceilMat);

                TransformComponent ceilTc;
                ceilTc.position   = {x + 0.5f, floorY + 1.0f, z + 0.5f};
                ceilTc.rotation.x = glm::half_pi<float>();
                ecsWorld.addComponent(ceiling, ceilTc);
            }

            // Walls
            if (enableWalls)
            {
                struct FaceDir { int dx, dz; float rotY; glm::vec3 offset; };
                const FaceDir faces[4] = {
                    {  0, -1, 0.0f,                     {0.5f, 0.5f, 0.0f} },
                    {  0,  1, glm::pi<float>(),          {0.5f, 0.5f, 1.0f} },
                    {  1,  0, glm::half_pi<float>(),     {1.0f, 0.5f, 0.5f} },
                    { -1,  0, -glm::half_pi<float>(),    {0.0f, 0.5f, 0.5f} },
                };
                for (const auto& face : faces)
                {
                    const int nx = x + face.dx;
                    const int nz = z + face.dz;
                    if (!floor.inBounds(nx, nz) || floor.get(nx, nz) == TileType::Wall)
                    {
                        Entity wall = ecsWorld.createEntity();
                        ecsWorld.addComponent(wall, FloorEntityTag{});

                        ProceduralMeshComponent wallMesh;
                        wallMesh.width  = 1.0f;
                        wallMesh.height = 1.0f;
                        ecsWorld.addComponent(wall, wallMesh);

                        MaterialComponent wallMat;
                        wallMat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
                        wallMat.doubleSided = false;
                        ecsWorld.addComponent(wall, wallMat);

                        TransformComponent wallTc;
                        wallTc.position   = glm::vec3(x, floorY, z) + face.offset;
                        wallTc.rotation.y = face.rotY;
                        ecsWorld.addComponent(wall, wallTc);
                    }
                }
            }

            // Floor transition trigger
            if (t == TileType::StairDown && floor.floorNumber < 3)
            {
                const GeneratedFloor* dest = dungeon.getFloor(floor.floorNumber + 1);
                if (!dest) continue;
                glm::ivec2 arrival = dest->playerStart;
                if (dest->hasStairUp())
                    arrival = findWalkableNeighbor(*dest, dest->stairUpPos);

                FloorTransitionTriggerComponent trig;
                trig.type        = FloorTransitionType::Stairs;
                trig.targetFloor = floor.floorNumber + 1;
                trig.arrivalGrid = arrival;
                ecsWorld.addComponent(e, trig);
            }
            else if (t == TileType::StairUp && floor.floorNumber > 0)
            {
                const GeneratedFloor* dest = dungeon.getFloor(floor.floorNumber - 1);
                if (!dest) continue;
                glm::ivec2 arrival = dest->playerStart;
                if (dest->hasStairDown())
                    arrival = findWalkableNeighbor(*dest, dest->stairDownPos);

                FloorTransitionTriggerComponent trig;
                trig.type        = FloorTransitionType::Stairs;
                trig.targetFloor = floor.floorNumber - 1;
                trig.arrivalGrid = arrival;
                ecsWorld.addComponent(e, trig);
            }
        }
    }
}

// ─── spawnEnemies ────────────────────────────────────────────────────────────
void DungeonFloorScene::spawnEnemies(SceneContext& /*ctx*/, const GeneratedFloor& floor)
{
    const std::string& RES      = GraphicsConstants::ENGINE_RESOURCES;
    const std::string  cubeMesh = RES + "/models/cube.obj";
    const std::string  enemyTex = RES + "/textures/orange_texture.png";
    const float        floorY   = DungeonConstants::floorWorldY(floor.floorNumber);

    for (const glm::ivec2& spawn : floor.enemySpawns)
    {
        Entity e = ecsWorld.createEntity();
        ecsWorld.addComponent(e, FloorEntityTag{});

        FloorEnemyComponent enemy;
        enemy.gridX  = spawn.x;
        enemy.gridZ  = spawn.y;
        enemy.floorY = floorY;
        enemy.patrolPath = generatePatrolPath(floor, spawn, 6);
        enemy.patrolIndex = 0;
        enemy.patrolForward = true;
        if (!enemy.patrolPath.empty())
        {
            enemy.fromPosition = {spawn.x + 0.5f, floorY + 0.5f, spawn.y + 0.5f};
            enemy.toPosition   = enemy.fromPosition;
        }
        ecsWorld.addComponent(e, enemy);

        StaticMeshComponent mesh;
        mesh.meshPath = cubeMesh;
        ecsWorld.addComponent(e, mesh);

        MaterialComponent mat;
        mat.texturePath = enemyTex;
        ecsWorld.addComponent(e, mat);

        TransformComponent tc;
        tc.position = {spawn.x + 0.5f, floorY + 0.5f, spawn.y + 0.5f};
        tc.scale    = {0.55f, 0.75f, 0.55f};
        ecsWorld.addComponent(e, tc);

        BoundsComponent bc;
        bc.min = {-0.3f, -0.4f, -0.3f};
        bc.max = { 0.3f,  0.4f,  0.3f};
        ecsWorld.addComponent(e, bc);
    }
}

// ─── spawnRamps ──────────────────────────────────────────────────────────────
// Spawns a visual ramp quad between this floor and the floor below at each
// StairDown position. The quad is a tilted ProceduralMesh that spans the
// vertical gap between the two floor levels.
void DungeonFloorScene::spawnRamps(SceneContext& /*ctx*/, const GeneratedFloor& floor)
{
    // Only floors that have a floor below get a ramp
    if (floor.floorNumber + 1 >= dungeon.getTotalFloorCount()) return;
    if (!floor.hasStairDown()) return;

    const std::string& RES    = GraphicsConstants::ENGINE_RESOURCES;
    const float        topY   = DungeonConstants::floorWorldY(floor.floorNumber);
    const float        botY   = DungeonConstants::floorWorldY(floor.floorNumber + 1);
    const float        height = topY - botY; // positive

    const glm::ivec2 stairPos = floor.stairDownPos;
    Entity e = ecsWorld.createEntity();
    ecsWorld.addComponent(e, FloorEntityTag{});

    // Ramp centre is halfway between the two floor levels
    const float midY  = (topY + botY) * 0.5f;
    const float centX = stairPos.x + 0.5f;
    const float centZ = stairPos.y + 0.5f;

    // Tilt angle: arctan(height / width_in_tiles) — using a 1-tile horizontal footprint
    const float rampAngle = std::atan2(height, 1.0f); // radians, rotate around Z

    ProceduralMeshComponent rampMesh;
    rampMesh.width  = 1.2f;
    rampMesh.height = std::sqrt(height * height + 1.0f * 1.0f) + 0.2f;
    ecsWorld.addComponent(e, rampMesh);

    MaterialComponent rampMat;
    rampMat.texturePath = RES + "/textures/grey_texture.png";
    rampMat.doubleSided = true;
    ecsWorld.addComponent(e, rampMat);

    TransformComponent rampTc;
    rampTc.position   = {centX, midY, centZ};
    // Lay the quad flat then tilt it: default quad is XY plane, we rotate
    // around X to make it face up, then tilt by the ramp angle around Z.
    rampTc.rotation.x = -glm::half_pi<float>() + rampAngle;
    ecsWorld.addComponent(e, rampTc);
}

// ─── spawnPlayer ─────────────────────────────────────────────────────────────
void DungeonFloorScene::spawnPlayer(SceneContext& ctx, glm::ivec2 startPos)
{
    const float worldX = startPos.x + 0.5f;
    const float worldZ = startPos.y + 0.5f;
    const float worldY = DungeonConstants::floorWorldY(0) + 0.5f; // floor 0 eye height

    playerEntity = ecsWorld.createEntity();
    // No FloorEntityTag — the player persists across floor transitions

    PlayerComponent pc;
    pc.gridX        = startPos.x;
    pc.gridZ        = startPos.y;
    pc.facing       = 1;
    pc.fromPosition = {worldX, worldY, worldZ};
    pc.toPosition   = {worldX, worldY, worldZ};
    pc.fromYaw      = 90.0f;
    pc.toYaw        = 90.0f;
    pc.inTransition = false;
    ecsWorld.addComponent(playerEntity, pc);

    TransformComponent playerTc;
    playerTc.position = {worldX, worldY, worldZ};
    ecsWorld.addComponent(playerEntity, playerTc);

    CameraDescriptionComponent camDesc;
    camDesc.active      = true;
    camDesc.fov         = glm::radians(70.0f);
    camDesc.aspectRatio = ctx.windowAspectRatio;
    camDesc.nearClip    = 0.05f;
    camDesc.farClip     = 100.0f;
    camDesc.target      = {worldX + 1.0f, worldY, worldZ};
    ecsWorld.addComponent(playerEntity, camDesc);

    // Visible marker (billboard above player — shown in debug bird's-eye view)
    const std::string& RES = GraphicsConstants::ENGINE_RESOURCES;

    Entity marker = ecsWorld.createEntity();
    // No FloorEntityTag — marker persists with the player

    ProceduralMeshComponent markerMesh;
    markerMesh.width  = 0.85f;
    markerMesh.height = 1.2f;
    ecsWorld.addComponent(marker, markerMesh);

    MaterialComponent markerMat;
    markerMat.texturePath = RES + "/pictures/furina.jpg";
    markerMat.doubleSided = true;
    ecsWorld.addComponent(marker, markerMat);

    TransformComponent markerTc;
    markerTc.position = glm::vec3(0.0f, 0.6f, 0.0f);
    ecsWorld.addComponent(marker, markerTc);

    HierarchyComponent hierarchy;
    hierarchy.parent = playerEntity;
    ecsWorld.addComponent(marker, hierarchy);
}

// ─── spawnDebugCamera ────────────────────────────────────────────────────────
void DungeonFloorScene::spawnDebugCamera(SceneContext& ctx)
{
    debugCamEntity = ecsWorld.createEntity();
    // No FloorEntityTag — debug camera persists across floor transitions

    // Use floor 0 dimensions for camera centering; position high enough to see all floors
    const GeneratedFloor& f0 = dungeon.getFloors().front();

    CameraDescriptionComponent dbgDesc;
    dbgDesc.active      = false;
    dbgDesc.fov         = glm::radians(60.0f);
    dbgDesc.aspectRatio = ctx.windowAspectRatio;
    dbgDesc.nearClip    = 0.1f;
    dbgDesc.farClip     = 200.0f;
    dbgDesc.target      = {
        f0.width  * 0.5f,
        -6.0f,
        f0.height * 0.5f
    };
    ecsWorld.addComponent(debugCamEntity, dbgDesc);

    TransformComponent dbgTc;
    dbgTc.position = {
        f0.width  * 0.5f,
        60.0f,
        f0.height * 0.5f
    };
    ecsWorld.addComponent(debugCamEntity, dbgTc);

    ecsWorld.addComponent(debugCamEntity, CameraControlOverrideComponent{});
}

// ─── generatePatrolPath ──────────────────────────────────────────────────────
std::vector<glm::ivec2> DungeonFloorScene::generatePatrolPath(const GeneratedFloor& floor,
                                                               glm::ivec2 start,
                                                               int length) const
{
    std::vector<glm::ivec2> path;
    path.reserve(static_cast<size_t>(length));
    path.push_back(start);

    glm::ivec2 dir = {1, 0};
    glm::ivec2 cur = start;
    for (int i = 1; i < length; ++i)
    {
        glm::ivec2 next = {cur.x + dir.x, cur.y + dir.y};
        if (!floor.isWalkable(next.x, next.y))
        {
            dir  = {-dir.x, -dir.y};
            next = {cur.x + dir.x, cur.y + dir.y};
            if (!floor.isWalkable(next.x, next.y))
                break;
        }
        path.push_back(next);
        cur = next;
    }

    return path;
}
