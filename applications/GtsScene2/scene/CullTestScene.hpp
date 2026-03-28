#pragma once

#include <cstdio>
#include <cstring>

#include "GtsScene.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "RenderCommandExtractor.hpp"

#include "TransformComponent.h"
#include "StaticMeshComponent.h"
#include "MaterialComponent.h"
#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraControlOverrideComponent.h"
#include "CameraOverrideComponent.h"
#include "GraphicsConstants.h"
#include "GtsKey.h"
#include "BitmapFont.h"
#include "BitmapFontLoader.h"
#include "UITextComponent.h"

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
    Entity     overlayEntity{};
    bool       overlayReady = false;

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

        // CameraOverrideComponent: FreeFlyCamera writes matrices directly, CameraGpuSystem skips this entity
        ecsWorld.addComponent(camera, CameraOverrideComponent{});

        // CameraControlOverrideComponent: DefaultCameraControlSystem skips this entity
        ecsWorld.addComponent(camera, CameraControlOverrideComponent{});
    }

public:
    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override
    {
        spawnGrid();
        spawnCamera(ctx.windowAspectRatio);

        // Load font and create overlay entity for culling/frustum state text.
        overlayFont = BitmapFontLoader::load(
            ctx.resources,
            GraphicsConstants::ENGINE_RESOURCES + "/fonts/retrofont.png",
            64, 40, 8, 8, 8,
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ",
            1.2f, true);

        overlayEntity = ecsWorld.createEntity();
        UITextComponent tc;
        tc.font    = &overlayFont;
        tc.x       = 0.01f;
        tc.y       = 0.01f;
        tc.scale   = 0.03f;
        tc.text    = "CULLING ON\nFRUSTUM LIVE";
        tc.visible = true;
        ecsWorld.addComponent(overlayEntity, tc);
        overlayReady = true;

        // FreeFlyCamera must run before CameraBindingSystem (installed by installRendererFeature)
        ecsWorld.addControllerSystem<FreeFlyCamera>();
        installRendererFeature();
    }

    void onUpdateSimulation(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
    }

    void onUpdateControllers(SceneContext& ctx) override
    {
        ecsWorld.updateControllers(ctx);

        auto* input     = ctx.inputSource;
        auto* extractor = ctx.extractor;

        // C — toggle frustum culling
        if (input->isKeyPressed(GtsKey::C))
        {
            extractor->setFrustumCullingEnabled(!extractor->isFrustumCullingEnabled());
            printf("\n[CULLING] Frustum culling %s\n",
                extractor->isFrustumCullingEnabled() ? "ON" : "OFF");
        }

        // F — freeze / unfreeze frustum (also used for camera down — one-frame overlap is acceptable)
        if (input->isKeyPressed(GtsKey::F))
        {
            extractor->toggleFrustumFreeze();
            if (extractor->isFrustumFrozen())
                printf("\n[FRUSTUM] Frustum FROZEN at current camera position\n");
            else
                printf("\n[FRUSTUM] Frustum LIVE\n");
        }

        // Update overlay text with current culling / frustum state.
        if (overlayReady)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "CULLING %s\nFRUSTUM %s",
                extractor->isFrustumCullingEnabled() ? "ON" : "OFF",
                extractor->isFrustumFrozen()         ? "FROZEN" : "LIVE");
            ecsWorld.getComponent<UITextComponent>(overlayEntity).text = buf;
        }

    }
};
