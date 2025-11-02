#pragma once
#include "ECSWorld.hpp"
#include "IResourceProvider.hpp"
#include "GtsScene.hpp"

#include "MeshComponent.h"
#include "UniformBufferComponent.h"
#include "MaterialComponent.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "AnimationComponent.h"

#include "CameraSystem.hpp"
#include "UniformDataSystem.hpp"
#include "TransformAnimationSystem.hpp"

// a run of the mill default scene in the engine, for testing purposes
class DefaultScene : public GtsScene
{
    public:
        // adds a simple rotating cube at the world center
        void firstCube(IResourceProvider& resource)
        {
            Entity cube = ecsWorld.createEntity();

            MeshComponent mc;
            mc.meshID = resource.requestMesh("resources/models/cube.obj");
            ecsWorld.addComponent<MeshComponent>(cube, mc);

            UniformBufferComponent ubc;
            ubc.uniformID = resource.requestUniformBuffer();
            ecsWorld.addComponent<UniformBufferComponent>(cube, ubc);

            MaterialComponent matc;
            matc.textureID = resource.requestTexture("resources/textures/green_texture.png");
            ecsWorld.addComponent<MaterialComponent>(cube, matc);

            TransformComponent tc;
            ecsWorld.addComponent<TransformComponent>(cube, tc);

            AnimationComponent anim;
            anim.enableMode(AnimationMode::Rotate);
            anim.rotationAxis = glm::vec3(0.5f, 1.0f, 0.2f);
            anim.rotationSpeed = glm::radians(90.0f);
            ecsWorld.addComponent<AnimationComponent>(cube, anim);
        }

        // add a second cube, that both translates and rotates
        void secondCube(IResourceProvider& resource)
        {
            Entity cube2 = ecsWorld.createEntity();

            MeshComponent mc2;
            mc2.meshID = resource.requestMesh("resources/models/cube.obj");
            ecsWorld.addComponent<MeshComponent>(cube2, mc2);

            UniformBufferComponent ubc2;
            ubc2.uniformID = resource.requestUniformBuffer();
            ecsWorld.addComponent<UniformBufferComponent>(cube2, ubc2);

            MaterialComponent matc2;
            matc2.textureID = resource.requestTexture("resources/textures/blue_texture.png");
            ecsWorld.addComponent<MaterialComponent>(cube2, matc2);

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

        // a third cube that scales
        void thirdCube(IResourceProvider& resource)
        {
            Entity cube2 = ecsWorld.createEntity();

            MeshComponent mc2;
            mc2.meshID = resource.requestMesh("resources/models/cube.obj");
            ecsWorld.addComponent<MeshComponent>(cube2, mc2);

            UniformBufferComponent ubc2;
            ubc2.uniformID = resource.requestUniformBuffer();
            ecsWorld.addComponent<UniformBufferComponent>(cube2, ubc2);

            MaterialComponent matc2;
            matc2.textureID = resource.requestTexture("resources/textures/purple_texture.png");
            ecsWorld.addComponent<MaterialComponent>(cube2, matc2);

            TransformComponent tc2;
            tc2.position = glm::vec3(-2.0f, -2.0f, -2.0f);
            ecsWorld.addComponent<TransformComponent>(cube2, tc2);

            AnimationComponent anim2;
            anim2.enableMode(AnimationMode::Scale);
            anim2.scaleAmplitude = glm::vec3(0.5f, 0.5f, 0.5f);
            anim2.scaleSpeed = 2.0f;
            ecsWorld.addComponent<AnimationComponent>(cube2, anim2);
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

        // add relevant systems to operate on our entities
        // no camera system would mean no picture after all :(
        void addSystems()
        {
            ecsWorld.addSystem<CameraSystem>();
            ecsWorld.addSystem<TransformAnimationSystem>();
            ecsWorld.addSystem<UniformDataSystem>();
        }

        // load in whatever we put into the scene
        void onLoad(IResourceProvider& resource)
        {
            firstCube(resource);
            secondCube(resource);
            thirdCube(resource);
            mainCamera(resource);
            addSystems();
        }

        // update call delegation
        void onUpdate(float dt) override
        {
            ecsWorld.update(dt);
        }
};