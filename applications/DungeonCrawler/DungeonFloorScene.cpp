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
#include "FloorEnemyComponent.h"
#include "StairComponent.h"
#include "DungeonTransitionState.h"

// Dungeon systems
#include "DungeonInputSystem.hpp"
#include "PlayerMovementSystem.hpp"
#include "PlayerCameraSystem.h"
#include "DungeonFreeFlyCamera.hpp"
#include "DungeonTileBindingSystem.hpp"
#include "EnemyMovementSystem.hpp"
#include "StairInteractionSystem.hpp"

// ─── Constructor ────────────────────────────────────────────────────────────
DungeonFloorScene::DungeonFloorScene(GeneratedFloor floor)
    : generatedFloor(std::move(floor))
{
}

// ─── onLoad ─────────────────────────────────────────────────────────────────
void DungeonFloorScene::onLoad(SceneContext& ctx,
                                const GtsSceneTransitionData* /*data*/)
{
    // On reload, release GPU resources via the clear systems before wiping the world.
    // Tag camera entities so CameraResourceClearSystem returns their UBO descriptor sets.
    // Tag renderable entities so RenderResourceClearSystem returns their SSBO slots.
    // destroyAfterClear = false because ecsWorld.clear() destroys the entities below.
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

    // All GPU resources released — safe to wipe the world
    ecsWorld.clear();
    playerEntity   = INVALID_ENTITY;
    debugCamEntity = INVALID_ENTITY;

    // Singletons shared across dungeon systems
    ecsWorld.createSingleton<DungeonInputComponent>();
    ecsWorld.createSingleton<DungeonGameStateComponent>();
    ecsWorld.createSingleton<DungeonFloorSingleton>();
    ecsWorld.getSingleton<DungeonFloorSingleton>().floor = &generatedFloor;

    // Spawn geometry, actors, and player
    spawnFloorTiles(ctx);
    spawnEnemies(ctx);

    // Determine player start position
    // Check the global transition state to see if we arrived via staircase
    DungeonTransitionState& ts = DungeonTransitionState::instance();
    glm::ivec2 startPos = generatedFloor.playerStart;

    if (ts.fromFloor >= 0)
    {
        // Coming from a lower floor (going Up) → start at stairDown
        if (ts.fromFloor > generatedFloor.floorNumber && !generatedFloor.stairDownPos.empty())
            startPos = generatedFloor.stairDownPos[0];
        // Coming from an upper floor (going Down) → start at stairUp
        else if (ts.fromFloor < generatedFloor.floorNumber && !generatedFloor.stairUpPos.empty())
            startPos = generatedFloor.stairUpPos[0];

        // Reset the transition state so stale data does not affect subsequent loads
        ts.fromFloor     = -1;
        ts.playerGridPos = {-1, -1};
    }

    spawnPlayer(ctx, startPos);
    spawnDebugCamera(ctx);

    // Application systems — order: input → movement → camera → debug cam → tile binding → enemy → stair
    ecsWorld.addControllerSystem<DungeonInputSystem>();
    ecsWorld.addControllerSystem<PlayerMovementSystem>();
    ecsWorld.addControllerSystem<PlayerCameraSystem>();
    ecsWorld.addControllerSystem<DungeonFreeFlyCamera>();
    ecsWorld.addControllerSystem<DungeonTileBindingSystem>();
    ecsWorld.addControllerSystem<EnemyMovementSystem>();
    ecsWorld.addControllerSystem<StairInteractionSystem>();

    // installRendererFeature must be LAST
    installRendererFeature();
}

// ─── onUpdateSimulation ─────────────────────────────────────────────────────
void DungeonFloorScene::onUpdateSimulation(SceneContext& ctx)
{
    ecsWorld.updateSimulation(ctx.time->deltaTime);
}

// ─── onUpdateControllers ────────────────────────────────────────────────────
void DungeonFloorScene::onUpdateControllers(SceneContext& ctx)
{
    ecsWorld.updateControllers(ctx);

    // T — toggle between first-person and debug bird's-eye camera
    const auto& input = ecsWorld.getSingleton<DungeonInputComponent>();
    if (input.toggleDebugCamera)
    {
        auto& playerCam = ecsWorld.getComponent<CameraDescriptionComponent>(playerEntity);
        auto& dbgCam    = ecsWorld.getComponent<CameraDescriptionComponent>(debugCamEntity);
        const bool wasPlayerActive = playerCam.active;
        playerCam.active = !wasPlayerActive;
        dbgCam.active    =  wasPlayerActive;
    }
}

// ─── spawnFloorTiles ────────────────────────────────────────────────────────
void DungeonFloorScene::spawnFloorTiles(SceneContext& /*ctx*/)
{
    for (int z = 0; z < generatedFloor.height; ++z)
    {
        for (int x = 0; x < generatedFloor.width; ++x)
        {
            const TileType t = generatedFloor.get(x, z);

            // Wall tiles are implicit — no entity
            if (t == TileType::Wall) continue;

            Entity e = ecsWorld.createEntity();

            DungeonTileComponent tile;
            tile.tileType    = t;
            tile.floorNumber = generatedFloor.floorNumber;
            ecsWorld.addComponent(e, tile);

            // Floor quad: lay flat (rotated -90° around X)
            TransformComponent tc;
            tc.position   = {x + 0.5f, 0.0f, z + 0.5f};
            tc.rotation.x = -glm::half_pi<float>();
            tc.scale      = {1.0f, 1.0f, 1.0f};
            ecsWorld.addComponent(e, tc);

            BoundsComponent bc;
            bc.min = {-0.5f, -0.5f, -0.02f};
            bc.max = { 0.5f,  0.5f,  0.02f};
            ecsWorld.addComponent(e, bc);

            // Ceiling — flipped above the floor tile (rotated +90° around X to face downward)
            if (enableCeiling)
            {
                Entity ceiling = ecsWorld.createEntity();

                ProceduralMeshComponent ceilMesh;
                ceilMesh.width  = 1.0f;
                ceilMesh.height = 1.0f;
                ecsWorld.addComponent(ceiling, ceilMesh);

                MaterialComponent ceilMat;
                ceilMat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
                ceilMat.doubleSided = true;
                ecsWorld.addComponent(ceiling, ceilMat);

                TransformComponent ceilTc;
                ceilTc.position   = {x + 0.5f, 1.0f, z + 0.5f};
                ceilTc.rotation.x = glm::half_pi<float>(); // face downward (opposite of floor)
                ecsWorld.addComponent(ceiling, ceilTc);
            }

            // Walls — spawn a vertical quad on each face adjacent to a wall cell
            if (enableWalls)
            {
                struct FaceDir { int dx, dz; float rotY; glm::vec3 offset; };
                const FaceDir faces[4] = {
                    {  0, -1, 0.0f,                     {0.5f, 0.5f, 0.0f} }, // North
                    {  0,  1, glm::pi<float>(),          {0.5f, 0.5f, 1.0f} }, // South
                    {  1,  0, glm::half_pi<float>(),     {1.0f, 0.5f, 0.5f} }, // East
                    { -1,  0, -glm::half_pi<float>(),    {0.0f, 0.5f, 0.5f} }, // West
                };
                for (const auto& face : faces)
                {
                    const int nx = x + face.dx;
                    const int nz = z + face.dz;
                    if (!generatedFloor.inBounds(nx, nz) || generatedFloor.get(nx, nz) == TileType::Wall)
                    {
                        Entity wall = ecsWorld.createEntity();

                        ProceduralMeshComponent wallMesh;
                        wallMesh.width  = 1.0f;
                        wallMesh.height = 1.0f;
                        ecsWorld.addComponent(wall, wallMesh);

                        MaterialComponent wallMat;
                        wallMat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
                        wallMat.doubleSided = false;
                        ecsWorld.addComponent(wall, wallMat);

                        TransformComponent wallTc;
                        wallTc.position   = glm::vec3(x, 0, z) + face.offset;
                        wallTc.rotation.y = face.rotY;
                        ecsWorld.addComponent(wall, wallTc);
                    }
                }
            }

            // Stair entities also get a StairComponent for interaction
            if (t == TileType::StairDown)
            {
                StairComponent sc;
                sc.direction   = StairDirection::Down;
                sc.targetFloor = generatedFloor.floorNumber + 1;
                ecsWorld.addComponent(e, sc);
            }
            else if (t == TileType::StairUp)
            {
                StairComponent sc;
                sc.direction   = StairDirection::Up;
                sc.targetFloor = generatedFloor.floorNumber - 1;
                ecsWorld.addComponent(e, sc);
            }
        }
    }
}

// ─── spawnEnemies ───────────────────────────────────────────────────────────
void DungeonFloorScene::spawnEnemies(SceneContext& /*ctx*/)
{
    const std::string& RES      = GraphicsConstants::ENGINE_RESOURCES;
    const std::string  cubeMesh = RES + "/models/cube.obj";
    const std::string  enemyTex = RES + "/textures/orange_texture.png";

    for (const glm::ivec2& spawn : generatedFloor.enemySpawns)
    {
        Entity e = ecsWorld.createEntity();

        FloorEnemyComponent enemy;
        enemy.gridX = spawn.x;
        enemy.gridZ = spawn.y;
        enemy.patrolPath = generatePatrolPath(spawn, 6);
        enemy.patrolIndex = 0;
        enemy.patrolForward = true;
        if (!enemy.patrolPath.empty())
        {
            enemy.fromPosition = {spawn.x + 0.5f, 0.5f, spawn.y + 0.5f};
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
        tc.position = {spawn.x + 0.5f, 0.5f, spawn.y + 0.5f};
        tc.scale    = {0.55f, 0.75f, 0.55f};
        ecsWorld.addComponent(e, tc);

        BoundsComponent bc;
        bc.min = {-0.3f, -0.4f, -0.3f};
        bc.max = { 0.3f,  0.4f,  0.3f};
        ecsWorld.addComponent(e, bc);
    }
}

// ─── spawnPlayer ────────────────────────────────────────────────────────────
void DungeonFloorScene::spawnPlayer(SceneContext& ctx, glm::ivec2 startPos)
{
    const float worldX = startPos.x + 0.5f;
    const float worldZ = startPos.y + 0.5f;

    playerEntity = ecsWorld.createEntity();

    PlayerComponent pc;
    pc.gridX        = startPos.x;
    pc.gridZ        = startPos.y;
    pc.facing       = 1; // East
    pc.fromPosition = {worldX, 0.5f, worldZ};
    pc.toPosition   = {worldX, 0.5f, worldZ};
    pc.fromYaw      = 90.0f;
    pc.toYaw        = 90.0f;
    pc.inTransition = false;
    ecsWorld.addComponent(playerEntity, pc);

    TransformComponent playerTc;
    playerTc.position = {worldX, 0.5f, worldZ};
    ecsWorld.addComponent(playerEntity, playerTc);

    CameraDescriptionComponent camDesc;
    camDesc.active      = true;
    camDesc.fov         = glm::radians(70.0f);
    camDesc.aspectRatio = ctx.windowAspectRatio;
    camDesc.nearClip    = 0.05f;
    camDesc.farClip     = 100.0f;
    camDesc.target      = {worldX + 1.0f, 0.5f, worldZ}; // initial look East
    ecsWorld.addComponent(playerEntity, camDesc);

    // Player marker — visible quad parented to the player entity via HierarchyComponent.
    // Renders as a billboard above the player position (visible in debug bird's-eye view).
    const std::string& RES = GraphicsConstants::ENGINE_RESOURCES;

    Entity marker = ecsWorld.createEntity();

    ProceduralMeshComponent markerMesh;
    markerMesh.width  = 0.85f;
    markerMesh.height = 1.2f;
    ecsWorld.addComponent(marker, markerMesh);

    MaterialComponent markerMat;
    markerMat.texturePath = RES + "/pictures/furina.jpg";
    markerMat.doubleSided = true;
    ecsWorld.addComponent(marker, markerMat);

    TransformComponent markerTc;
    markerTc.position = glm::vec3(0.0f, 0.6f, 0.0f); // local space: above player centre
    ecsWorld.addComponent(marker, markerTc);

    HierarchyComponent hierarchy;
    hierarchy.parent = playerEntity;
    ecsWorld.addComponent(marker, hierarchy);
}

// ─── spawnDebugCamera ───────────────────────────────────────────────────────
void DungeonFloorScene::spawnDebugCamera(SceneContext& ctx)
{
    debugCamEntity = ecsWorld.createEntity();

    CameraDescriptionComponent dbgDesc;
    dbgDesc.active      = false;
    dbgDesc.fov         = glm::radians(60.0f);
    dbgDesc.aspectRatio = ctx.windowAspectRatio;
    dbgDesc.nearClip    = 0.1f;
    dbgDesc.farClip     = 500.0f;
    dbgDesc.target      = {
        generatedFloor.width  * 0.5f,
        0.0f,
        generatedFloor.height * 0.5f
    };
    ecsWorld.addComponent(debugCamEntity, dbgDesc);

    TransformComponent dbgTc;
    dbgTc.position = {
        generatedFloor.width  * 0.5f,
        30.0f,
        generatedFloor.height * 0.5f
    };
    ecsWorld.addComponent(debugCamEntity, dbgTc);

    ecsWorld.addComponent(debugCamEntity, CameraControlOverrideComponent{});
}

// ─── generatePatrolPath ─────────────────────────────────────────────────────
std::vector<glm::ivec2> DungeonFloorScene::generatePatrolPath(glm::ivec2 start,
                                                               int length) const
{
    // Simple horizontal patrol: walk East up to `length` steps, then reverse
    std::vector<glm::ivec2> path;
    path.reserve(static_cast<size_t>(length));
    path.push_back(start);

    // Try extending East; fall back to West if needed
    glm::ivec2 dir = {1, 0};
    glm::ivec2 cur = start;
    for (int i = 1; i < length; ++i)
    {
        glm::ivec2 next = {cur.x + dir.x, cur.y + dir.y};
        if (!generatedFloor.isWalkable(next.x, next.y))
        {
            dir = {-dir.x, -dir.y}; // flip direction
            next = {cur.x + dir.x, cur.y + dir.y};
            if (!generatedFloor.isWalkable(next.x, next.y))
                break; // can't move in either direction
        }
        path.push_back(next);
        cur = next;
    }

    return path;
}
