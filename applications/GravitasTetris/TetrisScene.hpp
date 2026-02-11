#pragma once

#include "GtsScene.hpp"
#include "ECSWorld.hpp"

#include "TetrisInputState.hpp"
#include "TetrisInputSystem.hpp"
#include "TetrisGameSystem.hpp"
#include "TetrisVisualSystem.hpp"

#include "TetrisCameraSystem.hpp"
#include "UniformDataSystem.hpp"

#include "MeshComponent.h"
#include "UniformBufferComponent.h"
#include "MaterialComponent.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "AnimationComponent.h"

#include "GraphicsConstants.h"

// the central scene for the game, most delegation logic happens here
class TetrisScene : public GtsScene
{
    TetrisInputState inputState;

    // spawns a single cube with a given texture at a given coordinate
    Entity spawnCube(IResourceProvider& resource, const std::string& texturePath, const glm::vec3& position)
    {
        Entity e = ecsWorld.createEntity();

        MeshComponent mc;
        mc.meshID = resource.requestMesh(
            GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj");
        ecsWorld.addComponent<MeshComponent>(e, mc);

        UniformBufferComponent ubc;
        ubc.uniformID = resource.requestUniformBuffer();
        ecsWorld.addComponent<UniformBufferComponent>(e, ubc);

        MaterialComponent matc;
        matc.textureID = resource.requestTexture(texturePath);
        ecsWorld.addComponent<MaterialComponent>(e, matc);

        TransformComponent tc;
        tc.position = position;
        ecsWorld.addComponent<TransformComponent>(e, tc);

        return e;
    }

    // spawn cubes as a frame around the game field
    void buildTetrisFrame(IResourceProvider& resource)
    {
        constexpr int fieldWidth  = 10;
        constexpr int fieldHeight = 20;

        const std::string frameTexture = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";

        // left wall
        for (int y = 0; y < fieldHeight; ++y)
        {
            spawnCube(
                resource,
                frameTexture,
                glm::vec3(-1.0f, (float)y, 0.0f)
            );
        }

        // right wall
        for (int y = 0; y < fieldHeight; ++y)
        {
            spawnCube(
                resource,
                frameTexture,
                glm::vec3((float)fieldWidth, (float)y, 0.0f)
            );
        }

        // bottom wall
        for (int x = -1; x <= fieldWidth; ++x)
        {
            spawnCube(
                resource,
                frameTexture,
                glm::vec3((float)x, -1.0f, 0.0f)
            );
        }
    }

    // add a main camera
    void mainCamera(IResourceProvider& resource)
    {
        Entity camera = ecsWorld.createEntity();
        CameraComponent cc;
        ecsWorld.addComponent(camera, cc);

        TransformComponent ct;
        ct.position = glm::vec3(0.0f, 0.0f, 10.0f);
        ecsWorld.addComponent(camera, ct);
    }

public:
    // load in whatever we put into the scene
    void onLoad(SceneContext& ctx) override
    {
        buildTetrisFrame(*ctx.resources);
        mainCamera(*ctx.resources);

        ecsWorld.addControllerSystem<TetrisInputSystem>(inputState);
        ecsWorld.addSimulationSystem<TetrisGameSystem>(inputState);
        ecsWorld.addControllerSystem<TetrisVisualSystem>();
        ecsWorld.addSimulationSystem<TetrisCameraSystem>();
        ecsWorld.addSimulationSystem<UniformDataSystem>();
    }

    void onUpdate(SceneContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx.time->deltaTime);
        ecsWorld.updateControllers(ctx);
    }
};
