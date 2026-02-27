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
#include "CameraOverrideComponent.h"
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

    void buildTetrisFrame()
    {
        constexpr int fieldWidth  = 10;
        constexpr int fieldHeight = 20;

        const std::string frameTexture = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";

        for (int y = 0; y < fieldHeight; ++y)
            spawnCube(frameTexture, glm::vec3(-1.0f, (float)y, 0.0f));

        for (int y = 0; y < fieldHeight; ++y)
            spawnCube(frameTexture, glm::vec3((float)fieldWidth, (float)y, 0.0f));

        for (int x = -1; x <= fieldWidth; ++x)
            spawnCube(frameTexture, glm::vec3((float)x, -1.0f, 0.0f));
    }

    // Camera entity for the Tetris view.
    // Uses the override path: TetrisCameraSystem writes the orthographic matrices
    // directly into CameraGpuComponent each frame.  No CameraDescriptionComponent
    // is needed — CameraGpuSystem will never touch this entity.
    // void mainCamera()
    // {
    //     Entity camera = ecsWorld.createEntity();
    //     ecsWorld.addComponent(camera, CameraOverrideComponent{});
    //     ecsWorld.addComponent(camera, CameraGpuComponent{});
    // }

    void mainCamera()
    {
        Entity camera = ecsWorld.createEntity();

        CameraDescriptionComponent desc;
        desc.active = true;
        desc.target = glm::vec3(5.0f, 5.0f, 0.0f);
        ecsWorld.addComponent(camera, desc);

        TransformComponent ct;
        ct.position = glm::vec3(0.0f, 0.0f, 20.0f);
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
        mainCamera();

        addSingletonComponents();

        // TetrisCameraSystem must be registered before installRendererFeature so it
        // executes before CameraGpuSystem in the simulation pipeline, ensuring the
        // orthographic matrices are ready before CameraGpuSystem's pass.
        //ecsWorld.addSimulationSystem<TetrisCameraSystem>();

        installRendererFeature();

        ecsWorld.addControllerSystem<TetrisInputSystem>();
        ecsWorld.addSimulationSystem<TetrisGameSystem>();
        ecsWorld.addControllerSystem<TetrisVisualSystem>();
    }

    void onUpdate(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
        ecsWorld.updateControllers(ctx);
    }
};
