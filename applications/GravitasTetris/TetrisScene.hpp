#pragma once

#include "GtsScene.hpp"
#include "ECSWorld.hpp"

#include "TetrisInputComponent.hpp"
#include "TetrisInputSystem.hpp"
#include "TetrisGameSystem.hpp"
#include "TetrisVisualSystem.hpp"

#include "TetrisCameraSystem.hpp"

#include "RenderDescriptionComponent.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "CameraGpuComponent.h"

#include "GraphicsConstants.h"

// the central scene for the game, most delegation logic happens here
class TetrisScene : public GtsScene
{
    // spawns a single cube with a given texture at a given coordinate.
    // Only sets logical description and position — no GPU resource calls.
    Entity spawnCube(const std::string& texturePath, const glm::vec3& position)
    {
        Entity e = ecsWorld.createEntity();

        RenderDescriptionComponent desc;
        desc.meshPath    = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
        desc.texturePath = texturePath;
        ecsWorld.addComponent<RenderDescriptionComponent>(e, desc);

        TransformComponent tc;
        tc.position = position;
        ecsWorld.addComponent<TransformComponent>(e, tc);

        return e;
    }

    // spawn cubes as a frame around the game field
    void buildTetrisFrame()
    {
        constexpr int fieldWidth  = 10;
        constexpr int fieldHeight = 20;

        const std::string frameTexture = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";

        // left wall
        for (int y = 0; y < fieldHeight; ++y)
            spawnCube(frameTexture, glm::vec3(-1.0f, (float)y, 0.0f));

        // right wall
        for (int y = 0; y < fieldHeight; ++y)
            spawnCube(frameTexture, glm::vec3((float)fieldWidth, (float)y, 0.0f));

        // bottom wall
        for (int x = -1; x <= fieldWidth; ++x)
            spawnCube(frameTexture, glm::vec3((float)x, -1.0f, 0.0f));
    }

    // add a main camera — camera buffer allocation stays in scene setup
    void mainCamera(IResourceProvider& resource)
    {
        Entity camera = ecsWorld.createEntity();

        CameraComponent cc;
        cc.active = true;
        ecsWorld.addComponent(camera, cc);

        CameraGpuComponent cgc;
        cgc.viewID = resource.requestCameraBuffer();
        cgc.dirty = true;
        ecsWorld.addComponent(camera, cgc);

        TransformComponent ct;
        ct.position = glm::vec3(0.0f, 0.0f, 10.0f);
        ecsWorld.addComponent(camera, ct);
    }

    void addSingletonComponents()
    {
        ecsWorld.createSingleton<TetrisInputComponent>();
    }

public:
    void onLoad(SceneContext& ctx) override
    {
        buildTetrisFrame();
        mainCamera(*ctx.resources);

        addSingletonComponents();
        installRendererFeature();

        ecsWorld.addControllerSystem<TetrisInputSystem>();
        ecsWorld.addSimulationSystem<TetrisGameSystem>();
        ecsWorld.addControllerSystem<TetrisVisualSystem>();
        ecsWorld.addSimulationSystem<TetrisCameraSystem>();
    }

    void onUpdate(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
        ecsWorld.updateControllers(ctx);
    }
};
