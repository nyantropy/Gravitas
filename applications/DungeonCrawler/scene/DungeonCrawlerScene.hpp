#pragma once

#include <cmath>
#include <string>

#include "GlmConfig.h"
#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "GtsSceneTransitionData.h"
#include "SceneContext.h"
#include "GraphicsConstants.h"

// Engine components
#include "TransformComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "BoundsComponent.h"
#include "WorldImageComponent.h"
#include "RenderDescriptionComponent.h"
#include "UITextComponent.h"
#include "BitmapFont.h"
#include "BitmapFontLoader.h"

// Dungeon components
#include "DungeonMap.h"
#include "DungeonInputComponent.h"
#include "DungeonGameStateComponent.h"
#include "PlayerComponent.h"
#include "EnemyComponent.h"
#include "KeyItemComponent.h"
#include "ExitComponent.h"
#include "HudMarkerComponent.h"

// Dungeon systems
#include "DungeonInputSystem.hpp"
#include "PlayerMovementSystem.hpp"
#include "PlayerCameraSystem.h"
#include "DungeonFreeFlyCamera.hpp"
#include "EnemySystem.h"
#include "CombatSystem.h"
#include "HudSystem.h"

// ─────────────────────────────────────────────────────────────
// DungeonCrawlerScene
//
// Three-room dungeon prototype:
//   Room A (start)  — player begins here facing East
//   Room B (middle) — two patrol enemies; key item on the floor
//   Room C (exit)   — one patrol enemy; cyan exit tile in far corner
//
// Controls:
//   W/S/A/D  — move forward/back/strafe
//   Q / E    — turn left / right
//   Space    — attack cell ahead (1 damage per hit, 0.4 s cooldown)
//   T        — toggle debug bird's-eye camera
// ─────────────────────────────────────────────────────────────
class DungeonCrawlerScene : public GtsScene
{
    BitmapFont hudFont;       // must outlive UITextComponent entities

    Entity playerEntity{};
    Entity debugCamEntity{};

public:
    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override
    {
        // ── Singletons ──────────────────────────────────────────────────────
        ecsWorld.createSingleton<DungeonInputComponent>();
        ecsWorld.createSingleton<DungeonGameStateComponent>();

        // ── Texture paths ───────────────────────────────────────────────────
        const std::string& RES  = GraphicsConstants::ENGINE_RESOURCES;
        const std::string floor_tex   = RES + "/textures/green_texture.png";
        const std::string ceil_tex    = RES + "/textures/blue_texture.png";
        const std::string wall_tex    = RES + "/textures/grey_texture.png";
        const std::string enemy_tex   = RES + "/textures/orange_texture.png";
        const std::string key_tex     = RES + "/textures/yellow_texture.png";
        const std::string exit_tex    = RES + "/textures/cyan_texture.png";
        const std::string cube_mesh   = RES + "/models/cube.obj";

        // ── Floor + ceiling + walls ─────────────────────────────────────────
        // Helper lambda: spawn a WorldImageComponent flat quad.
        auto spawnQuad = [&](const std::string& tex,
                             float wx, float wy, float wz,
                             float rotX, float rotY,
                             float w = 1.0f, float h = 1.0f)
        {
            Entity e = ecsWorld.createEntity();

            WorldImageComponent img;
            img.texturePath = tex;
            img.width       = w;
            img.height      = h;
            ecsWorld.addComponent(e, img);

            TransformComponent tc;
            tc.position   = {wx, wy, wz};
            tc.rotation.x = rotX;
            tc.rotation.y = rotY;
            ecsWorld.addComponent(e, tc);

            BoundsComponent bc;
            bc.min = {-w * 0.5f, -h * 0.5f, -0.02f};
            bc.max = { w * 0.5f,  h * 0.5f,  0.02f};
            ecsWorld.addComponent(e, bc);
        };

        for (int z = 0; z < MAP_H; ++z)
        {
            for (int x = 0; x < MAP_W; ++x)
            {
                if (DUNGEON_MAP[z][x] != 0) continue;

                const float cx = x + 0.5f;
                const float cz = z + 0.5f;

                // Floor — lay flat on XZ plane
                spawnQuad(floor_tex, cx, 0.0f, cz,
                          -glm::half_pi<float>(), 0.0f);

                // Ceiling — flipped above the floor
                spawnQuad(ceil_tex, cx, 1.0f, cz,
                           glm::half_pi<float>(), 0.0f);

                // North wall (neighbor at z-1 is solid)
                if (z == 0 || DUNGEON_MAP[z-1][x] != 0)
                    spawnQuad(wall_tex, cx, 0.5f, float(z),
                              0.0f, 0.0f);

                // South wall (neighbor at z+1 is solid)
                if (z == MAP_H-1 || DUNGEON_MAP[z+1][x] != 0)
                    spawnQuad(wall_tex, cx, 0.5f, float(z+1),
                              0.0f, 0.0f);

                // East wall (neighbor at x+1 is solid)
                if (x == MAP_W-1 || DUNGEON_MAP[z][x+1] != 0)
                    spawnQuad(wall_tex, float(x+1), 0.5f, cz,
                              0.0f, glm::half_pi<float>());

                // West wall (neighbor at x-1 is solid)
                if (x == 0 || DUNGEON_MAP[z][x-1] != 0)
                    spawnQuad(wall_tex, float(x), 0.5f, cz,
                              0.0f, glm::half_pi<float>());
            }
        }

        // ── Player entity ───────────────────────────────────────────────────
        // Start: Room A, cell (3, 2), facing East (+X).
        playerEntity = ecsWorld.createEntity();

        PlayerComponent pc;
        pc.gridX  = 3;
        pc.gridZ  = 2;
        pc.facing = 1; // East — corridor is to the east, so orient that way
        ecsWorld.addComponent(playerEntity, pc);

        TransformComponent playerTc;
        playerTc.position = {3.5f, 0.5f, 2.5f};
        ecsWorld.addComponent(playerEntity, playerTc);

        CameraDescriptionComponent camDesc;
        camDesc.active      = true;
        camDesc.fov         = glm::radians(70.0f);
        camDesc.aspectRatio = ctx.windowAspectRatio;
        camDesc.nearClip    = 0.05f;
        camDesc.farClip     = 100.0f;
        camDesc.target      = {4.5f, 0.5f, 2.5f}; // initial look East
        ecsWorld.addComponent(playerEntity, camDesc);

        // ── Debug free-fly camera ───────────────────────────────────────────
        debugCamEntity = ecsWorld.createEntity();

        CameraDescriptionComponent dbgDesc;
        dbgDesc.active      = false;
        dbgDesc.fov         = glm::radians(60.0f);
        dbgDesc.aspectRatio = ctx.windowAspectRatio;
        dbgDesc.nearClip    = 0.1f;
        dbgDesc.farClip     = 500.0f;
        dbgDesc.target      = {10.0f, 0.0f, 10.0f};
        ecsWorld.addComponent(debugCamEntity, dbgDesc);

        TransformComponent dbgTc;
        dbgTc.position = {10.0f, 22.0f, 10.0f};
        ecsWorld.addComponent(debugCamEntity, dbgTc);

        ecsWorld.addComponent(debugCamEntity, CameraControlOverrideComponent{});

        // ── Enemies ─────────────────────────────────────────────────────────
        // Helper: spawn a cube-mesh enemy that patrols between two waypoints.
        auto spawnEnemy = [&](int gx, int gz,
                               int p0x, int p0z,
                               int p1x, int p1z,
                               float cooldownOffset)
        {
            Entity e = ecsWorld.createEntity();

            EnemyComponent ec;
            ec.gridX      = gx;
            ec.gridZ      = gz;
            ec.patrolX0   = p0x; ec.patrolZ0 = p0z;
            ec.patrolX1   = p1x; ec.patrolZ1 = p1z;
            ec.moveCooldown = cooldownOffset; // stagger so they don't all move at once
            ecsWorld.addComponent(e, ec);

            RenderDescriptionComponent rd;
            rd.meshPath    = cube_mesh;
            rd.texturePath = enemy_tex;
            ecsWorld.addComponent(e, rd);

            TransformComponent tc;
            tc.position = {gx + 0.5f, 0.5f, gz + 0.5f};
            tc.scale    = {0.55f, 0.75f, 0.55f};
            ecsWorld.addComponent(e, tc);

            BoundsComponent bc;
            bc.min = {-0.3f, -0.4f, -0.3f};
            bc.max = { 0.3f,  0.4f,  0.3f};
            ecsWorld.addComponent(e, bc);
        };

        // Enemy 1 — Room B, horizontal patrol
        spawnEnemy(10, 8,   8, 8,  13, 8,   0.0f);
        // Enemy 2 — Room B, horizontal patrol (offset to stagger)
        spawnEnemy(11, 10,  8, 10, 14, 10,  0.6f);
        // Enemy 3 — Room C, horizontal patrol
        spawnEnemy(13, 16, 10, 16, 17, 16,  0.3f);

        // ── Key item ─────────────────────────────────────────────────────────
        // Sits in Room B at cell (12, 9). Yellow vertical quad at eye height.
        {
            Entity ke = ecsWorld.createEntity();

            KeyItemComponent kc;
            kc.gridX = 12; kc.gridZ = 9;
            ecsWorld.addComponent(ke, kc);

            spawnQuad(key_tex, 12.5f, 0.35f, 9.5f,
                      0.0f, 0.0f, 0.35f, 0.35f);
            // Note: visual quad is separate from the KeyItemComponent entity
            // (simpler — key pickup destroys the KeyItemComponent entity,
            //  the visual quad stays as a "remnant" after pickup, which is
            //  acceptable for a prototype)
        }

        // ── Exit tile ────────────────────────────────────────────────────────
        // Floor-level cyan quad at cell (15, 17) in Room C.
        {
            Entity xe = ecsWorld.createEntity();

            ExitComponent xc;
            xc.gridX = 15; xc.gridZ = 17;
            ecsWorld.addComponent(xe, xc);

            // Cyan floor overlay (slightly above floor to avoid z-fighting)
            spawnQuad(exit_tex, 15.5f, 0.01f, 17.5f,
                      -glm::half_pi<float>(), 0.0f);
        }

        // ── HUD ──────────────────────────────────────────────────────────────
        hudFont = BitmapFontLoader::load(
            ctx.resources,
            RES + "/fonts/retrofont.png",
            64, 40, 8, 8, 8,
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ",
            1.2f, true);

        // Health display — top-left
        {
            Entity he = ecsWorld.createEntity();
            HudMarkerComponent hm;
            hm.type = HudMarkerComponent::Type::Health;
            ecsWorld.addComponent(he, hm);

            UITextComponent tc;
            tc.font    = &hudFont;
            tc.x       = 0.02f;
            tc.y       = 0.02f;
            tc.scale   = 0.035f;
            tc.text    = "HP  5 OF 5";
            tc.visible = true;
            ecsWorld.addComponent(he, tc);
        }

        // Status display — below health (key / win / lose)
        {
            Entity se = ecsWorld.createEntity();
            HudMarkerComponent sm;
            sm.type = HudMarkerComponent::Type::Status;
            ecsWorld.addComponent(se, sm);

            UITextComponent tc;
            tc.font    = &hudFont;
            tc.x       = 0.02f;
            tc.y       = 0.07f;
            tc.scale   = 0.035f;
            tc.text    = "KEY  NO";
            tc.visible = true;
            ecsWorld.addComponent(se, tc);
        }

        // Message display — centre screen, shown briefly
        {
            Entity me = ecsWorld.createEntity();
            HudMarkerComponent mm;
            mm.type = HudMarkerComponent::Type::Message;
            ecsWorld.addComponent(me, mm);

            UITextComponent tc;
            tc.font    = &hudFont;
            tc.x       = 0.3f;
            tc.y       = 0.45f;
            tc.scale   = 0.045f;
            tc.text    = "";
            tc.visible = false;
            ecsWorld.addComponent(me, tc);
        }

        // ── Systems ──────────────────────────────────────────────────────────
        // Order matters: input → movement → camera → enemies → combat → hud
        ecsWorld.addControllerSystem<DungeonInputSystem>();
        ecsWorld.addControllerSystem<PlayerMovementSystem>();
        ecsWorld.addControllerSystem<PlayerCameraSystem>();
        ecsWorld.addControllerSystem<DungeonFreeFlyCamera>();
        ecsWorld.addControllerSystem<EnemySystem>();
        ecsWorld.addControllerSystem<CombatSystem>();
        ecsWorld.addControllerSystem<HudSystem>();
        installRendererFeature();
    }

    void onUpdateSimulation(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
    }

    void onUpdateControllers(SceneContext& ctx) override
    {
        ecsWorld.updateControllers(ctx);

        // T — toggle between first-person and debug bird's-eye camera.
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
};
