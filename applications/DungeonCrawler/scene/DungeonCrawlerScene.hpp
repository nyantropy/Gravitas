#pragma once

#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "GtsSceneTransitionData.h"

#include "RenderDescriptionComponent.h"
#include "CameraDescriptionComponent.h"
#include "TransformComponent.h"

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

            Entity camera = ecsWorld.createEntity();

            CameraDescriptionComponent camDesc;
            camDesc.active = true;
            ecsWorld.addComponent(camera, camDesc);

            TransformComponent ct;
            ct.position = glm::vec3(0.0f, 0.0f, 10.0f);
            ecsWorld.addComponent(camera, ct);

            installRendererFeature();
        }

        void onUpdate(SceneContext& ctx) override
        {
            ecsWorld.updateSimulation(ctx.time->deltaTime);
            ecsWorld.updateControllers(ctx);
        }
};
