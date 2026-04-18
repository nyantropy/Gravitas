#pragma once

#include <cstdio>
#include <cstring>

#include "GtsScene.hpp"
#include "ECSWorld.hpp"
#include "TransformComponent.h"
#include "StaticMeshComponent.h"
#include "MaterialComponent.h"
#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "RetroFontAtlas.h"
#include "CameraControlOverrideComponent.h"
#include "GraphicsConstants.h"
#include "GtsKey.h"
#include "UiSystem.h"
#include "UiHandle.h"
#include "BitmapFont.h"
#include "BitmapFontLoader.h"

#include "FreeFlyCamera.hpp"

// Frustum culling test scene.
// Spawns a 20x20 grid of cubes (400 total) on the XZ plane, 3 units apart,
// centered at the world origin.  A free-fly camera lets the user move around.
//
// Keys:
//   C      — toggle frustum culling on / off
//   F      — freeze / unfreeze the cull frustum at the current camera position
//   W/S/A/D/Q/E/R/F — camera movement (see FreeFlyCamera)
class CullTestScene : public GtsScene
{
    static constexpr int   GRID_DIM     = 20;           // cubes per axis
    static constexpr float GRID_SPACING = 3.0f;         // world units between cube centres

    BitmapFont overlayFont;
    UiHandle   overlayHandle = UI_INVALID_HANDLE;
    bool       cullingEnabled = true;
    bool       frustumFrozen = false;

    void spawnGrid()
    {
        for (int x = 0; x < GRID_DIM; ++x)
        {
            for (int z = 0; z < GRID_DIM; ++z)
            {
                float worldX = (x - (GRID_DIM - 1) * 0.5f) * GRID_SPACING;
                float worldZ = (z - (GRID_DIM - 1) * 0.5f) * GRID_SPACING;

                Entity e = ecsWorld.createEntity();

                TransformComponent tc;
                tc.position = glm::vec3(worldX, 0.0f, worldZ);
                ecsWorld.addComponent(e, tc);

                StaticMeshComponent mesh;
                mesh.meshPath = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
                ecsWorld.addComponent(e, mesh);

                MaterialComponent mat;
                mat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/green_texture.png";
                ecsWorld.addComponent(e, mat);

                ecsWorld.addComponent(e, BoundsComponent{});
            }
        }
    }

    void spawnCamera(float aspectRatio)
    {
        Entity camera = ecsWorld.createEntity();

        CameraDescriptionComponent desc;
        desc.active      = true;
        desc.fov         = glm::radians(60.0f);
        desc.aspectRatio = aspectRatio;
        desc.nearClip    = 0.1f;
        desc.farClip     = 500.0f;
        ecsWorld.addComponent(camera, desc);

        // Start position: elevated, looking toward the grid
        TransformComponent tc;
        tc.position = glm::vec3(0.0f, 8.0f, 40.0f);
        ecsWorld.addComponent(camera, tc);

        // CameraControlOverrideComponent: DefaultCameraControlSystem skips this entity
        ecsWorld.addComponent(camera, CameraControlOverrideComponent{});
    }

public:
    void onLoad(EcsControllerContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override
    {
        spawnGrid();
        spawnCamera(ctx.windowAspectRatio);

        if (ctx.input != nullptr)
        {
            ctx.input->bind("gts_scene2.toggle_culling",
                            InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::C)},
                            ActivationMode::Pressed);
            ctx.input->bind("gts_scene2.toggle_frustum_freeze",
                            InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::F)},
                            ActivationMode::Pressed);
            ctx.input->bind("gts_scene2.camera_forward",
                            InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::W)},
                            ActivationMode::Held);
            ctx.input->bind("gts_scene2.camera_backward",
                            InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::S)},
                            ActivationMode::Held);
            ctx.input->bind("gts_scene2.camera_left",
                            InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::A)},
                            ActivationMode::Held);
            ctx.input->bind("gts_scene2.camera_right",
                            InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::D)},
                            ActivationMode::Held);
            ctx.input->bind("gts_scene2.camera_yaw_left",
                            InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::Q)},
                            ActivationMode::Held);
            ctx.input->bind("gts_scene2.camera_yaw_right",
                            InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::E)},
                            ActivationMode::Held);
            ctx.input->bind("gts_scene2.camera_up",
                            InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::R)},
                            ActivationMode::Held);
            ctx.input->bind("gts_scene2.camera_down",
                            InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::T)},
                            ActivationMode::Held);
        }

        // Load font and create overlay entity for culling/frustum state text.
        overlayFont = BitmapFontLoader::load(
            ctx.resources,
            GraphicsConstants::ENGINE_RESOURCES + "/fonts/retrofont.png",
            RetroFontAtlas::ATLAS_W, RetroFontAtlas::ATLAS_H,
            RetroFontAtlas::CELL_W, RetroFontAtlas::CELL_H, RetroFontAtlas::ATLAS_COLS,
            std::string(RetroFontAtlas::CHAR_ORDER),
            RetroFontAtlas::LINE_HEIGHT, true);

        overlayHandle = ctx.ui->createNode(UiNodeType::Text);
        UiLayoutSpec overlayLayout;
        overlayLayout.positionMode = UiPositionMode::Absolute;
        overlayLayout.widthMode = UiSizeMode::Fixed;
        overlayLayout.heightMode = UiSizeMode::Fixed;
        overlayLayout.offsetMin = {0.01f, 0.01f};
        ctx.ui->setLayout(overlayHandle, overlayLayout);
        ctx.ui->setState(overlayHandle, UiStateFlags{
            .visible = true,
            .enabled = false,
            .interactable = false
        });
        ctx.ui->setPayload(overlayHandle, UiTextData{
            "CULLING ON\nFRUSTUM LIVE",
            {},
            {1.0f, 1.0f, 1.0f, 1.0f},
            0.03f
        });
        ctx.ui->setTextFont(overlayHandle, &overlayFont);

        // FreeFlyCamera must run before CameraBindingSystem (installed by installRendererFeature)
        ecsWorld.addControllerSystem<FreeFlyCamera>();
        installRendererFeature(ctx);
    }

    void onUpdateSimulation(const EcsSimulationContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx);
    }

    void onUpdateControllers(const EcsControllerContext& ctx) override
    {
        ecsWorld.updateControllers(ctx);

        // C — toggle frustum culling
        if (ctx.input->isPressed("gts_scene2.toggle_culling"))
        {
            cullingEnabled = !cullingEnabled;
            ctx.engineCommands->requestSetFrustumCullingEnabled(cullingEnabled);
            printf("\n[CULLING] Frustum culling %s\n",
                cullingEnabled ? "ON" : "OFF");
        }

        // F — freeze / unfreeze frustum (also used for camera down — one-frame overlap is acceptable)
        if (ctx.input->isPressed("gts_scene2.toggle_frustum_freeze"))
        {
            frustumFrozen = !frustumFrozen;
            ctx.engineCommands->requestSetFrustumFreeze(frustumFrozen);
            if (frustumFrozen)
                printf("\n[FRUSTUM] Frustum FROZEN at current camera position\n");
            else
                printf("\n[FRUSTUM] Frustum LIVE\n");
        }

        // Update overlay text with current culling / frustum state.
        if (overlayHandle != UI_INVALID_HANDLE)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "CULLING %s\nFRUSTUM %s",
                cullingEnabled ? "ON" : "OFF",
                frustumFrozen  ? "FROZEN" : "LIVE");
            ctx.ui->setPayload(overlayHandle, UiTextData{
                buf,
                {},
                {1.0f, 1.0f, 1.0f, 1.0f},
                0.03f
            });
        }

    }
};
