#include "StairTestScene.h"

#include <cmath>
#include <limits>

// Engine components
#include "GlmConfig.h"
#include "TransformComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "ProceduralMeshComponent.h"
#include "MaterialComponent.h"
#include "BoundsComponent.h"
#include "GraphicsConstants.h"
#include "HierarchyComponent.h"

// Dungeon components
#include "DungeonFloorSingleton.h"
#include "DungeonInputComponent.h"
#include "PlayerComponent.h"
#include "DungeonTileComponent.h"
#include "FloorEntityTag.h"
#include "FloorTransitionStateComponent.h"
#include "FloorTransitionTriggerComponent.h"
#include "DungeonConstants.h"

// Dungeon systems
#include "DungeonInputSystem.hpp"
#include "PlayerMovementSystem.hpp"
#include "PlayerCameraSystem.h"
#include "FloorTransitionSystem.hpp"
#include "DungeonFreeFlyCamera.hpp"
#include "DungeonTileBindingSystem.hpp"

// ─── onLoad ──────────────────────────────────────────────────────────────────
void StairTestScene::onLoad(SceneContext& ctx, const GtsSceneTransitionData* /*data*/)
{
    ecsWorld.clear();
    playerEntity   = INVALID_ENTITY;
    debugCamEntity = INVALID_ENTITY;

    ecsWorld.createSingleton<DungeonInputComponent>();
    ecsWorld.createSingleton<FloorTransitionStateComponent>();

    buildTestFloors();

    auto& sg = ecsWorld.createSingleton<DungeonFloorSingleton>();
    sg.allFloors[0]      = &testFloors[0];
    sg.allFloors[1]      = &testFloors[1];
    sg.floor             = &testFloors[0];
    sg.currentFloorIndex = 0;

    spawnFloor(0);
    spawnFloor(1);
    spawnRamp();
    spawnPlayer(ctx);
    spawnDebugCamera(ctx);

    // Systems — order: input → movement → camera → transition → debug cam → tile binding
    ecsWorld.addControllerSystem<DungeonInputSystem>();
    ecsWorld.addControllerSystem<PlayerMovementSystem>();
    ecsWorld.addControllerSystem<PlayerCameraSystem>();
    ecsWorld.addControllerSystem<FloorTransitionSystem>();
    ecsWorld.addControllerSystem<DungeonFreeFlyCamera>();
    ecsWorld.addControllerSystem<DungeonTileBindingSystem>();

    installRendererFeature();
}

// ─── onUpdateSimulation ──────────────────────────────────────────────────────
void StairTestScene::onUpdateSimulation(SceneContext& ctx)
{
    ecsWorld.updateSimulation(ctx.time->deltaTime);
}

// ─── onUpdateControllers ─────────────────────────────────────────────────────
void StairTestScene::onUpdateControllers(SceneContext& ctx)
{
    ecsWorld.updateControllers(ctx);

    // T — toggle between first-person and debug bird's-eye camera
    // (blocked during floor transitions)
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
}

// ─── buildTestFloors ─────────────────────────────────────────────────────────
void StairTestScene::buildTestFloors()
{
    // Floor 0
    testFloors[0].floorNumber = 0;
    testFloors[0].width       = 10;
    testFloors[0].height      = 10;
    testFloors[0].tiles.assign(100, TileType::Wall);
    testFloors[0].playerStart = {2, 5};

    for (int z = 0; z < 10; ++z)
        for (int x = 0; x < 10; ++x)
        {
            const int v = FLOOR0[z][x];
            TileType t = (v == 0) ? TileType::Floor
                       : (v == 2) ? TileType::StairDown
                       :             TileType::Wall;
            testFloors[0].set(x, z, t);
        }
    testFloors[0].stairDownPos.push_back({8, 5});

    // Floor 1
    testFloors[1].floorNumber = 1;
    testFloors[1].width       = 10;
    testFloors[1].height      = 10;
    testFloors[1].tiles.assign(100, TileType::Wall);
    testFloors[1].playerStart = {2, 5};

    for (int z = 0; z < 10; ++z)
        for (int x = 0; x < 10; ++x)
        {
            const int v = FLOOR1[z][x];
            TileType t = (v == 0) ? TileType::Floor
                       : (v == 3) ? TileType::StairUp
                       :             TileType::Wall;
            testFloors[1].set(x, z, t);
        }
    testFloors[1].stairUpPos.push_back({1, 5});
}

// ─── spawnFloor ──────────────────────────────────────────────────────────────
void StairTestScene::spawnFloor(int floorIdx)
{
    const GeneratedFloor& fl    = testFloors[floorIdx];
    const float           baseY = -static_cast<float>(floorIdx) * FLOOR_Y;

    for (int z = 0; z < fl.height; ++z)
    {
        for (int x = 0; x < fl.width; ++x)
        {
            const TileType t = fl.get(x, z);
            if (t == TileType::Wall) continue;

            Entity e = ecsWorld.createEntity();
            ecsWorld.addComponent(e, FloorEntityTag{});

            DungeonTileComponent tile;
            tile.tileType    = t;
            tile.floorNumber = floorIdx;
            ecsWorld.addComponent(e, tile);

            TransformComponent tc;
            tc.position   = {x + 0.5f, baseY, z + 0.5f};
            tc.rotation.x = -glm::half_pi<float>();
            ecsWorld.addComponent(e, tc);

            BoundsComponent bc;
            bc.min = {-0.5f, -0.5f, -0.02f};
            bc.max = { 0.5f,  0.5f,  0.02f};
            ecsWorld.addComponent(e, bc);

            // Stair-down at (8,5) on floor 0 → arrive at (2,5) on floor 1
            if (t == TileType::StairDown)
            {
                FloorTransitionTriggerComponent trig;
                trig.type        = FloorTransitionType::Stairs;
                trig.targetFloor = 1;
                trig.arrivalGrid = {2, 5};
                ecsWorld.addComponent(e, trig);
            }
            // Stair-up at (1,5) on floor 1 → arrive at (7,5) on floor 0
            else if (t == TileType::StairUp)
            {
                FloorTransitionTriggerComponent trig;
                trig.type        = FloorTransitionType::Stairs;
                trig.targetFloor = 0;
                trig.arrivalGrid = {7, 5};
                ecsWorld.addComponent(e, trig);
            }
        }
    }
}

// ─── spawnRamp ───────────────────────────────────────────────────────────────
// Visual ramp at the floor 0 stair-down position, tilted to span the
// vertical gap down to floor 1.
void StairTestScene::spawnRamp()
{
    const float topY      = 0.0f;
    const float botY      = -FLOOR_Y;
    const float height    = topY - botY;
    const float rampAngle = std::atan2(height, 1.0f); // radians

    Entity e = ecsWorld.createEntity();
    ecsWorld.addComponent(e, FloorEntityTag{});

    ProceduralMeshComponent mesh;
    mesh.width  = 1.2f;
    mesh.height = std::sqrt(height * height + 1.0f) + 0.2f;
    ecsWorld.addComponent(e, mesh);

    MaterialComponent mat;
    mat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
    mat.doubleSided = true;
    ecsWorld.addComponent(e, mat);

    TransformComponent tc;
    tc.position   = {8.5f, (topY + botY) * 0.5f, 5.5f};
    tc.rotation.x = -glm::half_pi<float>() + rampAngle;
    ecsWorld.addComponent(e, tc);
}

// ─── spawnPlayer ─────────────────────────────────────────────────────────────
void StairTestScene::spawnPlayer(SceneContext& ctx)
{
    const float worldX = 2.5f;
    const float worldY = DungeonConstants::floorWorldY(0) + 0.5f;
    const float worldZ = 5.5f;

    playerEntity = ecsWorld.createEntity();

    PlayerComponent pc;
    pc.gridX        = 2;
    pc.gridZ        = 5;
    pc.facing       = 1; // East
    pc.fromPosition = {worldX, worldY, worldZ};
    pc.toPosition   = {worldX, worldY, worldZ};
    pc.fromYaw      = 90.0f;
    pc.toYaw        = 90.0f;
    ecsWorld.addComponent(playerEntity, pc);

    TransformComponent tc;
    tc.position = {worldX, worldY, worldZ};
    ecsWorld.addComponent(playerEntity, tc);

    CameraDescriptionComponent cam;
    cam.active      = true;
    cam.fov         = glm::radians(70.0f);
    cam.aspectRatio = ctx.windowAspectRatio;
    cam.nearClip    = 0.05f;
    cam.farClip     = 100.0f;
    cam.target      = {worldX + 1.0f, worldY, worldZ};
    ecsWorld.addComponent(playerEntity, cam);

    // Visible billboard marker (shown in debug bird's-eye view)
    Entity marker = ecsWorld.createEntity();

    ProceduralMeshComponent markerMesh;
    markerMesh.width  = 0.85f;
    markerMesh.height = 1.2f;
    ecsWorld.addComponent(marker, markerMesh);

    MaterialComponent markerMat;
    markerMat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/pictures/furina.jpg";
    markerMat.doubleSided = true;
    ecsWorld.addComponent(marker, markerMat);

    TransformComponent markerTc;
    markerTc.position = {0.0f, 0.6f, 0.0f};
    ecsWorld.addComponent(marker, markerTc);

    HierarchyComponent hierarchy;
    hierarchy.parent = playerEntity;
    ecsWorld.addComponent(marker, hierarchy);
}

// ─── spawnDebugCamera ────────────────────────────────────────────────────────
void StairTestScene::spawnDebugCamera(SceneContext& ctx)
{
    debugCamEntity = ecsWorld.createEntity();

    CameraDescriptionComponent cam;
    cam.active      = false;
    cam.fov         = glm::radians(60.0f);
    cam.aspectRatio = ctx.windowAspectRatio;
    cam.nearClip    = 0.1f;
    cam.farClip     = 200.0f;
    cam.target      = {5.0f, -2.0f, 5.0f};
    ecsWorld.addComponent(debugCamEntity, cam);

    TransformComponent tc;
    tc.position = {5.0f, 20.0f, 5.0f};
    ecsWorld.addComponent(debugCamEntity, tc);

    ecsWorld.addComponent(debugCamEntity, CameraControlOverrideComponent{});
}
