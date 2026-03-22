#pragma once

#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "GtsSceneTransitionData.h"

#include "RenderDescriptionComponent.h"
#include "CameraDescriptionComponent.h"
#include "TransformComponent.h"
#include "BoundsComponent.h"
#include "UiImageComponent.h"
#include "WorldImageComponent.h"

#include "SceneContext.h"
#include "GraphicsConstants.h"

class DungeonCrawlerScene : public GtsScene
{
    public:
        void onLoad(SceneContext& ctx,
                    const GtsSceneTransitionData* data = nullptr) override
        {
            Entity cube = ecsWorld.createEntity();

            RenderDescriptionComponent desc;
            desc.meshPath    = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
            desc.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/green_texture.png";
            ecsWorld.addComponent<RenderDescriptionComponent>(cube, desc);

            TransformComponent tc;
            ecsWorld.addComponent<TransformComponent>(cube, tc);
            ecsWorld.addComponent<BoundsComponent>(cube, BoundsComponent{});

            Entity camera = ecsWorld.createEntity();

            CameraDescriptionComponent camDesc;
            camDesc.active = true;
            ecsWorld.addComponent(camera, camDesc);

            TransformComponent ct;
            ct.position = glm::vec3(0.0f, 0.0f, 10.0f);
            ecsWorld.addComponent(camera, ct);

            texture_id_type furinaTexID =
                ctx.resources->requestTexture(
                    GraphicsConstants::ENGINE_RESOURCES + "/pictures/furina.jpg");

            Entity furinaEntity = ecsWorld.createEntity();
            UiImageComponent img;
            img.textureID   = furinaTexID;
            img.x           = 0.1f;
            img.y           = 0.1f;
            img.width       = 0.3f;
            img.imageAspect = 850.0f / 1200.0f;
            img.visible     = true;
            ecsWorld.addComponent(furinaEntity, img);

            // World-space Furina quad (3 units to the right of the cube)
            // furina.jpg is 850 x 1200 px → aspect = 850/1200
            // width = 2.0 world units, height = width / aspect
            constexpr float furinaAspect = 850.0f / 1200.0f;
            constexpr float furinaW      = 2.0f;
            constexpr float furinaH      = furinaW / furinaAspect;

            Entity worldFurina = ecsWorld.createEntity();

            WorldImageComponent wimg;
            wimg.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/pictures/furina.jpg";
            wimg.width       = furinaW;
            wimg.height      = furinaH;
            ecsWorld.addComponent(worldFurina, wimg);

            TransformComponent wt;
            wt.position = glm::vec3(3.0f, 0.0f, 0.0f);
            ecsWorld.addComponent(worldFurina, wt);

            BoundsComponent wb;
            wb.min = glm::vec3(-furinaW * 0.5f, -furinaH * 0.5f, -0.01f);
            wb.max = glm::vec3( furinaW * 0.5f,  furinaH * 0.5f,  0.01f);
            ecsWorld.addComponent(worldFurina, wb);

            installRendererFeature();
        }

        void onUpdateSimulation(SceneContext& ctx) override
        {
            ecsWorld.updateSimulation(ctx.time->deltaTime);
        }

        void onUpdateControllers(SceneContext& ctx) override
        {
            ecsWorld.updateControllers(ctx);
        }
};
