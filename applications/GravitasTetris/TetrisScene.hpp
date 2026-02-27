#pragma once

#include "GtsScene.hpp"
#include "ECSWorld.hpp"

#include "TetrisInputComponent.hpp"
#include "TetrisInputSystem.hpp"
#include "TetrisGameSystem.hpp"
#include "TetrisVisualSystem.hpp"
#include "TetrisCameraSystem.hpp"

#include "RenderDescriptionComponent.h"
#include "CameraDescriptionComponent.h"
#include "CameraOverrideComponent.h"
#include "TransformComponent.h"

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

    void mainCamera()
    {
        Entity camera = ecsWorld.createEntity();

        CameraDescriptionComponent desc;
        desc.active      = true;
        desc.fov         = glm::radians(60.0f);
        desc.aspectRatio = 800.0f / 800.0f;
        desc.nearClip    = 1.0f;
        desc.farClip     = 1000.0f;
        ecsWorld.addComponent(camera, desc);

        ecsWorld.addComponent(camera, TransformComponent{});

        ecsWorld.addComponent(camera, CameraOverrideComponent{});
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
        installRendererFeature();

        ecsWorld.addControllerSystem<TetrisInputSystem>();
        ecsWorld.addSimulationSystem<TetrisGameSystem>();
        ecsWorld.addControllerSystem<TetrisVisualSystem>();

        // Registered after TetrisGameSystem so it runs on fully updated game state.
        // CameraBindingSystem is a controller and always follows all simulation,
        // so the perspective matrices are guaranteed to be ready before GPU upload.
        ecsWorld.addSimulationSystem<TetrisCameraSystem>();
    }

    void onUpdate(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
        ecsWorld.updateControllers(ctx);
    }
};
