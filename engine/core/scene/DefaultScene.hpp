#pragma once
#include "ECSWorld.hpp"
#include "IResourceProvider.hpp"
#include "GtsScene.hpp"

#include "MeshComponent.h"
#include "ObjectGpuComponent.h"
#include "CameraGpuComponent.h"
#include "MaterialComponent.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "AnimationComponent.h"

#include "CameraSystem.hpp"
#include "CameraControlSystem.hpp"

#include "TransformAnimationSystem.hpp"

#include "SceneContext.h"

#include "GraphicsConstants.h"

// a run of the mill default scene in the engine, for testing purposes
class DefaultScene : public GtsScene
{
    private:
        Entity controlledCube;
    public:
        // adds a simple rotating cube at the world center
        void firstCube(IResourceProvider& resource)
        {
            controlledCube = ecsWorld.createEntity();

            MeshComponent mc;
            mc.meshID = resource.requestMesh(GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj");
            ecsWorld.addComponent<MeshComponent>(controlledCube, mc);

            ObjectGpuComponent ogc;
            ogc.objectSSBOIndex = resource.requestObjectSlot();
            ogc.dirty = true;
            ecsWorld.addComponent<ObjectGpuComponent>(controlledCube, ogc);

            MaterialComponent matc;
            matc.textureID = resource.requestTexture(GraphicsConstants::ENGINE_RESOURCES + "/textures/green_texture.png");
            ecsWorld.addComponent<MaterialComponent>(controlledCube, matc);

            TransformComponent tc;
            ecsWorld.addComponent<TransformComponent>(controlledCube, tc);

            AnimationComponent anim;
            anim.enableMode(AnimationMode::Rotate);
            anim.rotationAxis = glm::vec3(0.5f, 1.0f, 0.2f);
            anim.rotationSpeed = glm::radians(90.0f);
            ecsWorld.addComponent<AnimationComponent>(controlledCube, anim);
        }

        // add a second cube, that both translates and rotates
        void secondCube(IResourceProvider& resource)
        {
            Entity cube2 = ecsWorld.createEntity();

            MeshComponent mc2;
            mc2.meshID = resource.requestMesh(GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj");
            ecsWorld.addComponent<MeshComponent>(cube2, mc2);

            ObjectGpuComponent ogc2;
            ogc2.objectSSBOIndex = resource.requestObjectSlot();
            ogc2.dirty = true;
            ecsWorld.addComponent<ObjectGpuComponent>(cube2, ogc2);

            MaterialComponent matc2;
            matc2.textureID = resource.requestTexture(GraphicsConstants::ENGINE_RESOURCES + "/textures/blue_texture.png");
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
            mc2.meshID = resource.requestMesh(GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj");
            ecsWorld.addComponent<MeshComponent>(cube2, mc2);

            ObjectGpuComponent ogc2;
            ogc2.objectSSBOIndex = resource.requestObjectSlot();
            ogc2.dirty = true;
            ecsWorld.addComponent<ObjectGpuComponent>(cube2, ogc2);

            MaterialComponent matc2;
            matc2.textureID = resource.requestTexture(GraphicsConstants::ENGINE_RESOURCES + "/textures/purple_texture.png");
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
            cc.active = true;
            ecsWorld.addComponent(camera, cc);

            CameraGpuComponent cgc;
            cgc.buffer = resource.requestUniformBuffer(sizeof(CameraUBO));
            cgc.dirty = true;
            ecsWorld.addComponent(camera, cgc);

            TransformComponent ct;
            ct.position = glm::vec3(0.0f, 0.0f, 10.0f);
            ecsWorld.addComponent(camera, ct);
        }

        // add relevant systems to operate on our entities
        // no camera system would mean no picture after all :(
        void addSystems()
        {
            installRendererFeature();
            ecsWorld.addControllerSystem<CameraControlSystem>();
            ecsWorld.addSimulationSystem<CameraSystem>();
            ecsWorld.addSimulationSystem<TransformAnimationSystem>();
        }

        // load in whatever we put into the scene
        void onLoad(SceneContext& ctx) override
        {
            firstCube(*ctx.resources);
            secondCube(*ctx.resources);
            thirdCube(*ctx.resources);
            mainCamera(*ctx.resources);
            addSystems();
        }

        // update call delegation
        void onUpdate(SceneContext& ctx) override
        {
            // first the controllers, then the simulation
            ecsWorld.updateControllers(ctx);
            ecsWorld.updateSimulation(ctx.time->deltaTime);
            

            if (ctx.input->isKeyPressed(GtsKey::X)) 
            {
                std::cout << "X pressed" << std::endl;
                ctx.engineCommands->requestPause();
            }

            if (ctx.input->isKeyPressed(GtsKey::Y)) 
            {
                std::cout << "Y pressed" << std::endl;
                ctx.engineCommands->requestResume();
            }
        }

};
