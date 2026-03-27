#pragma once

#include <cmath>

#include "GlmConfig.h"
#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "GtsSceneTransitionData.h"
#include "SceneContext.h"
#include "GraphicsConstants.h"

#include "TransformComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "BoundsComponent.h"
#include "WorldImageComponent.h"

#include "DungeonMap.h"
#include "DungeonInputComponent.h"
#include "PlayerComponent.h"
#include "DungeonInputSystem.hpp"
#include "PlayerMovementSystem.hpp"
#include "PlayerCameraSystem.h"
#include "DungeonFreeFlyCamera.hpp"

class DungeonCrawlerScene : public GtsScene
{
    Entity playerEntity{};
    Entity debugCamEntity{};
    Entity markerEntity{};

public:
    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override
    {
        // Input singleton — written by DungeonInputSystem, read by all dungeon systems.
        ecsWorld.createSingleton<DungeonInputComponent>();

        // ── Floor quads ────────────────────────────────────────────────────
        // One WorldImageComponent per walkable cell, rotated flat onto the XZ plane.
        const std::string floorTex =
            GraphicsConstants::ENGINE_RESOURCES + "/textures/green_texture.png";

        for (int z = 0; z < MAP_H; ++z)
        {
            for (int x = 0; x < MAP_W; ++x)
            {
                if (DUNGEON_MAP[z][x] != 0) continue;

                Entity floor = ecsWorld.createEntity();

                WorldImageComponent img;
                img.texturePath = floorTex;
                img.width       = 1.0f;
                img.height      = 1.0f;
                ecsWorld.addComponent(floor, img);

                TransformComponent tc;
                tc.position   = glm::vec3(x + 0.5f, 0.0f, z + 0.5f);
                tc.rotation.x = -glm::half_pi<float>(); // lay flat on the XZ plane
                ecsWorld.addComponent(floor, tc);

                BoundsComponent fb;
                fb.min = glm::vec3(-0.5f, -0.5f, -0.01f);
                fb.max = glm::vec3( 0.5f,  0.5f,  0.01f);
                ecsWorld.addComponent(floor, fb);
            }
        }

        // ── Player entity (also owns the first-person camera) ──────────────
        playerEntity = ecsWorld.createEntity();

        PlayerComponent pc;
        pc.gridX  = 9;  // center corridor area — isWalkable(9,9) == true
        pc.gridZ  = 9;
        pc.facing = 0; // facing North (-Z)
        ecsWorld.addComponent(playerEntity, pc);

        TransformComponent playerTc;
        playerTc.position = glm::vec3(9.5f, 0.5f, 9.5f);
        ecsWorld.addComponent(playerEntity, playerTc);

        CameraDescriptionComponent camDesc;
        camDesc.active      = true;
        camDesc.fov         = glm::radians(70.0f);
        camDesc.aspectRatio = ctx.windowAspectRatio;
        camDesc.nearClip    = 0.05f;
        camDesc.farClip     = 100.0f;
        camDesc.target      = glm::vec3(9.5f, 0.5f, 8.5f); // initial look North
        ecsWorld.addComponent(playerEntity, camDesc);

        // ── Debug free-fly camera ──────────────────────────────────────────
        // Starts inactive; toggled by T. DungeonFreeFlyCamera updates desc.target,
        // CameraGpuSystem builds the view matrix. DefaultCameraControlSystem is
        // skipped via CameraControlOverrideComponent.
        debugCamEntity = ecsWorld.createEntity();

        CameraDescriptionComponent dbgDesc;
        dbgDesc.active      = false;
        dbgDesc.fov         = glm::radians(60.0f);
        dbgDesc.aspectRatio = ctx.windowAspectRatio;
        dbgDesc.nearClip    = 0.1f;
        dbgDesc.farClip     = 500.0f;
        dbgDesc.target      = glm::vec3(10.0f, 0.0f, 10.0f); // initial look: down at map center
        ecsWorld.addComponent(debugCamEntity, dbgDesc);

        TransformComponent dbgTc;
        dbgTc.position = glm::vec3(10.0f, 22.0f, 10.0f); // high above map center
        ecsWorld.addComponent(debugCamEntity, dbgTc);

        ecsWorld.addComponent(debugCamEntity, CameraControlOverrideComponent{});

        // ── Furina position marker ─────────────────────────────────────────
        // A flat WorldImageComponent quad on the floor at the player's grid cell.
        constexpr float furinaAspect = 850.0f / 1200.0f;
        constexpr float markerW      = 0.85f;
        constexpr float markerH      = markerW / furinaAspect;

        markerEntity = ecsWorld.createEntity();

        WorldImageComponent markerImg;
        markerImg.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/pictures/furina.jpg";
        markerImg.width       = markerW;
        markerImg.height      = markerH;
        ecsWorld.addComponent(markerEntity, markerImg);

        TransformComponent markerTc;
        markerTc.position   = glm::vec3(9.5f, 0.02f, 9.5f);
        markerTc.rotation.x = -glm::half_pi<float>(); // flat on floor
        ecsWorld.addComponent(markerEntity, markerTc);

        BoundsComponent mb;
        mb.min = glm::vec3(-markerW * 0.5f, -markerH * 0.5f, -0.01f);
        mb.max = glm::vec3( markerW * 0.5f,  markerH * 0.5f,  0.01f);
        ecsWorld.addComponent(markerEntity, mb);

        // ── Systems ────────────────────────────────────────────────────────
        // DungeonInputSystem must run first — it populates DungeonInputComponent
        // before movement and camera systems read from it.
        ecsWorld.addControllerSystem<DungeonInputSystem>();
        ecsWorld.addControllerSystem<PlayerMovementSystem>();
        ecsWorld.addControllerSystem<PlayerCameraSystem>();
        ecsWorld.addControllerSystem<DungeonFreeFlyCamera>();
        installRendererFeature();
    }

    void onUpdateSimulation(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
    }

    void onUpdateControllers(SceneContext& ctx) override
    {
        ecsWorld.updateControllers(ctx);

        // T — toggle between player camera and debug free-fly camera.
        // Read from DungeonInputComponent so no GtsAction pollution.
        const auto& input = ecsWorld.getSingleton<DungeonInputComponent>();
        if (input.toggleDebugCamera)
        {
            auto& playerCam = ecsWorld.getComponent<CameraDescriptionComponent>(playerEntity);
            auto& dbgCam    = ecsWorld.getComponent<CameraDescriptionComponent>(debugCamEntity);
            bool wasPlayerActive = playerCam.active;
            playerCam.active = !wasPlayerActive;
            dbgCam.active    =  wasPlayerActive;
        }

        // Sync Furina marker to player grid position each frame.
        const auto& player = ecsWorld.getComponent<PlayerComponent>(playerEntity);
        auto& markerTc     = ecsWorld.getComponent<TransformComponent>(markerEntity);
        markerTc.position  = glm::vec3(
            player.gridX + 0.5f,
            0.02f,
            player.gridZ + 0.5f);
    }
};
