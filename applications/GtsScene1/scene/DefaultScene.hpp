#pragma once

#include "ECSWorld.hpp"
#include "GtsScene.hpp"

#include "StaticMeshComponent.h"
#include "MaterialComponent.h"
#include "CameraDescriptionComponent.h"
#include "TransformComponent.h"
#include "AnimationComponent.h"

#include "TransformAnimationSystem.hpp"

#include "SceneContext.h"
#include "GraphicsConstants.h"

// a run of the mill default scene in the engine, for testing purposes
class DefaultScene : public GtsScene
{
    private:
        Entity controlledCube;
    public:
        void firstCube()
        {
            controlledCube = ecsWorld.createEntity();

            StaticMeshComponent mesh;
            mesh.meshPath = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
            ecsWorld.addComponent<StaticMeshComponent>(controlledCube, mesh);

            MaterialComponent mat;
            mat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/green_texture.png";
            ecsWorld.addComponent<MaterialComponent>(controlledCube, mat);

            TransformComponent tc;
            ecsWorld.addComponent<TransformComponent>(controlledCube, tc);

            AnimationComponent anim;
            anim.enableMode(AnimationMode::Rotate);
            anim.rotationAxis = glm::vec3(0.5f, 1.0f, 0.2f);
            anim.rotationSpeed = glm::radians(90.0f);
            ecsWorld.addComponent<AnimationComponent>(controlledCube, anim);
        }

        void secondCube()
        {
            Entity cube2 = ecsWorld.createEntity();

            StaticMeshComponent mesh;
            mesh.meshPath = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
            ecsWorld.addComponent<StaticMeshComponent>(cube2, mesh);

            MaterialComponent mat;
            mat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/blue_texture.png";
            ecsWorld.addComponent<MaterialComponent>(cube2, mat);

            TransformComponent tc2;
            tc2.position = glm::vec3(2.0f, 2.0f, 2.0f);
            ecsWorld.addComponent<TransformComponent>(cube2, tc2);

            AnimationComponent anim2;
            anim2.enableMode(AnimationMode::Translate);
            anim2.enableMode(AnimationMode::Rotate);
            anim2.translationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
            anim2.translationAmplitude = 1.0f;
            anim2.translationSpeed = 2.0f;
            anim2.rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);
            anim2.rotationSpeed = glm::radians(30.0f);
            ecsWorld.addComponent<AnimationComponent>(cube2, anim2);
        }

        void thirdCube()
        {
            Entity cube3 = ecsWorld.createEntity();

            StaticMeshComponent mesh;
            mesh.meshPath = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
            ecsWorld.addComponent<StaticMeshComponent>(cube3, mesh);

            MaterialComponent mat;
            mat.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/purple_texture.png";
            ecsWorld.addComponent<MaterialComponent>(cube3, mat);

            TransformComponent tc3;
            tc3.position = glm::vec3(-2.0f, -2.0f, -2.0f);
            ecsWorld.addComponent<TransformComponent>(cube3, tc3);

            AnimationComponent anim3;
            anim3.enableMode(AnimationMode::Scale);
            anim3.scaleAmplitude = glm::vec3(0.5f, 0.5f, 0.5f);
            anim3.scaleSpeed = 2.0f;
            ecsWorld.addComponent<AnimationComponent>(cube3, anim3);
        }

        // gameplay-only camera setup — no GPU resource calls
        void mainCamera()
        {
            Entity camera = ecsWorld.createEntity();

            CameraDescriptionComponent desc;
            desc.active = true;
            ecsWorld.addComponent(camera, desc);

            TransformComponent ct;
            ct.position = glm::vec3(0.0f, 0.0f, 10.0f);
            ecsWorld.addComponent(camera, ct);
        }

        void addSystems()
        {
            installRendererFeature();
            ecsWorld.addSimulationSystem<TransformAnimationSystem>();
        }

        void onLoad(SceneContext& ctx,
                    const GtsSceneTransitionData* data = nullptr) override
        {
            firstCube();
            secondCube();
            thirdCube();
            mainCamera();
            addSystems();
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
