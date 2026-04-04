#include "StairTestScene.h"

#include <cmath>
#include <limits>

// Engine components
#include "GlmConfig.h"
#include "TransformComponent.h"
#include "CameraDescriptionComponent.h"
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

// Dungeon systems
#include "DungeonInputSystem.hpp"
#include "PlayerMovementSystem.hpp"
#include "PlayerCameraSystem.h"
#include "FloorTransitionSystem.hpp"
#include "DungeonTileBindingSystem.hpp"

// ─── onLoad ──────────────────────────────────────────────────────────────────
void StairTestScene::onLoad(SceneContext& ctx, const GtsSceneTransitionData* /*data*/)
{
    resetSceneWorld();
    playerEntity   = INVALID_ENTITY;

    ecsWorld.createSingleton<DungeonInputComponent>();
    ecsWorld.createSingleton<FloorTransitionStateComponent>();

    buildTestFloors();

    auto& sg = ecsWorld.createSingleton<DungeonFloorSingleton>();
    sg.allFloors[0]      = &testFloors[0];
    sg.allFloors[1]      = &testFloors[1];
    sg.floor             = &testFloors[0];
    sg.currentFloorIndex = 0;

    // World-space origin per floor: (X offset, base Y, Z offset)
    sg.floorWorldOffset[0] = {0.0f,  0.0f, 0.0f};
    sg.floorWorldOffset[1] = {14.0f, -2.0f, 0.0f};

    spawnFloor(0);
    spawnFloor(1);
    spawnRamp();
    spawnPlayer(ctx);

    // Systems — order: input → movement → camera → transition → debug cam → tile binding
    ecsWorld.addControllerSystem<DungeonInputSystem>();
    ecsWorld.addControllerSystem<PlayerMovementSystem>();
    ecsWorld.addControllerSystem<PlayerCameraSystem>();
    ecsWorld.addControllerSystem<FloorTransitionSystem>();
    ecsWorld.addControllerSystem<DungeonTileBindingSystem>();

    installRendererFeature(ctx);
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

}

// ─── buildTestFloors ─────────────────────────────────────────────────────────
void StairTestScene::buildTestFloors()
{
    // Floor A: 10×10, interior walkable, border walls except transition tile at east edge (9,5)
    testFloors[0].floorNumber = 0;
    testFloors[0].width       = 10;
    testFloors[0].height      = 10;
    testFloors[0].tiles.assign(100, TileType::Wall);
    testFloors[0].playerStart = {2, 5};

    for (int z = 0; z < 10; ++z)
        for (int x = 0; x < 10; ++x)
        {
            const bool isInterior      = (x >= 1 && x <= 8 && z >= 1 && z <= 8);
            const bool isTransitionTile = (x == 9 && z == 5);
            if (isInterior || isTransitionTile)
                testFloors[0].set(x, z, TileType::Floor);
        }

    // Floor B: 10×10, interior walkable, border walls except transition tile at west edge (0,5)
    testFloors[1].floorNumber = 1;
    testFloors[1].width       = 10;
    testFloors[1].height      = 10;
    testFloors[1].tiles.assign(100, TileType::Wall);
    testFloors[1].playerStart = {1, 5};

    for (int z = 0; z < 10; ++z)
        for (int x = 0; x < 10; ++x)
        {
            const bool isInterior      = (x >= 1 && x <= 8 && z >= 1 && z <= 8);
            const bool isTransitionTile = (x == 0 && z == 5);
            if (isInterior || isTransitionTile)
                testFloors[1].set(x, z, TileType::Floor);
        }
}

// ─── spawnFloor ──────────────────────────────────────────────────────────────
void StairTestScene::spawnFloor(int floorIdx)
{
    const GeneratedFloor& fl      = testFloors[floorIdx];
    const float           baseY   = (floorIdx == 0) ? 0.0f : -2.0f;
    const float           xOffset = (floorIdx == 0) ? 0.0f : 14.0f;

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
            tc.position   = {x + xOffset + 0.5f, baseY, z + 0.5f};
            tc.rotation.x = -glm::half_pi<float>();
            ecsWorld.addComponent(e, tc);

            BoundsComponent bc;
            bc.min = {-0.5f, -0.5f, -0.02f};
            bc.max = { 0.5f,  0.5f,  0.02f};
            ecsWorld.addComponent(e, bc);

            // Transition trigger on east edge of Floor A at (9,5)
            // Arrival: one step inside Floor B → local (1,5), world (15.5, -1.5, 5.5)
            if (floorIdx == 0 && x == 9 && z == 5)
            {
                FloorTransitionTriggerComponent trig;
                trig.type        = FloorTransitionType::Stairs;
                trig.targetFloor = 1;
                trig.arrivalGrid = {1, 5};
                ecsWorld.addComponent(e, trig);
            }

            // Transition trigger on west edge of Floor B at (0,5)
            // Arrival: one step inside Floor A → local (8,5), world (8.5, 0.5, 5.5)
            if (floorIdx == 1 && x == 0 && z == 5)
            {
                FloorTransitionTriggerComponent trig;
                trig.type        = FloorTransitionType::Stairs;
                trig.targetFloor = 0;
                trig.arrivalGrid = {8, 5};
                ecsWorld.addComponent(e, trig);
            }
        }
    }
}

// ─── spawnRamp ───────────────────────────────────────────────────────────────
// Single quad spanning the open space between Floor A and Floor B.
// Horizontal span: 4 tiles (world X 10→14), vertical drop: 2 units (Y 0→-2),
// depth: 5 tiles (Z 2→7). Textured orange for visibility.
void StairTestScene::spawnRamp()
{
    constexpr float hSpan = 4.0f; // horizontal distance
    constexpr float yDrop = 2.0f; // vertical drop
    constexpr float depth = 5.0f; // ramp depth along Z

    const float rampLen = std::sqrt(hSpan * hSpan + yDrop * yDrop);
    const float angle   = std::atan2(yDrop, hSpan); // radians

    Entity ramp = ecsWorld.createEntity();
    ecsWorld.addComponent(ramp, FloorEntityTag{});

    ProceduralMeshComponent mesh;
    mesh.width  = rampLen; // along the slope
    mesh.height = depth;   // into the screen (Z axis)
    ecsWorld.addComponent(ramp, mesh);

    MaterialComponent mat;
    mat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/orange_texture.png";
    mat.doubleSided = true;
    ecsWorld.addComponent(ramp, mat);

    // Center: X=12 (midpoint of 10..14), Y=-1 (midpoint of 0..-2), Z=4.5 (midpoint of 2..7)
    // getModelMatrix applies rotations X→Y→Z, so vertex sees Z first, then Y, then X.
    // To "first lay flat (Rx(-pi/2)), then tilt slope (Rz(-a))" in world space we need the
    // combined matrix Rz(-a)*Rx(-pi/2), which factors as Rx(-pi/2)*Ry(+a) in XYZ order.
    TransformComponent tc;
    tc.position   = {12.0f, -1.0f, 4.5f};
    tc.rotation.x = -glm::half_pi<float>(); // lay quad flat (same as floor tiles)
    tc.rotation.y = angle;                   // gentle downward slope: right side (Floor B) goes down
    ecsWorld.addComponent(ramp, tc);

    BoundsComponent bc;
    bc.min = {-rampLen * 0.5f, -depth * 0.5f, -0.1f};
    bc.max = { rampLen * 0.5f,  depth * 0.5f,  0.1f};
    ecsWorld.addComponent(ramp, bc);
}

// ─── spawnPlayer ─────────────────────────────────────────────────────────────
void StairTestScene::spawnPlayer(SceneContext& ctx)
{
    constexpr float worldX = 2.5f;
    constexpr float worldY = 0.5f; // Floor A base Y=0, eye height=0.5
    constexpr float worldZ = 5.5f;

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
    cam.target      = {3.5f, worldY, worldZ}; // looking east (+X)
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
