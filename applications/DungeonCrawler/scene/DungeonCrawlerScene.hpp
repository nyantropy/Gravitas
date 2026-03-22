#pragma once

#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "GtsSceneTransitionData.h"

#include "RenderDescriptionComponent.h"
#include "CameraDescriptionComponent.h"
#include "TransformComponent.h"
#include "BoundsComponent.h"
#include "UiImageComponent.h"

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
            img.textureID = furinaTexID;
            img.x         = 0.1f;
            img.y         = 0.1f;
            img.w         = 0.3f;
            img.h         = 0.5f;
            img.visible   = true;
            ecsWorld.addComponent(furinaEntity, img);

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
