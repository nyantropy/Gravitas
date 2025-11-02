#pragma once
#include "ECSWorld.hpp"
#include "IResourceProvider.hpp"
#include "GtsScene.hpp"

#include "MeshComponent.h"
#include "UniformBufferComponent.h"
#include "MaterialComponent.h"
#include "TransformComponent.h"
#include "CameraComponent.h"

#include "CameraSystem.hpp"
#include "UniformDataSystem.hpp"
#include "TransformAnimationSystem.hpp"

// a run of the mill default scene in the engine, for testing purposes
class DefaultScene : public GtsScene
{
    public:
        void onLoad(IResourceProvider& resource)
        {
            //lets make a cube
            Entity cube = ecsWorld.createEntity();

            //and add a mesh component to it
            MeshComponent mc;
            mc.meshID = resource.requestMesh("resources/models/cube.obj");
            ecsWorld.addComponent<MeshComponent>(cube, mc);

            //now we can try adding a uniform buffer component as well
            UniformBufferComponent ubc;
            ubc.uniformID = resource.requestUniformBuffer();
            ecsWorld.addComponent<UniformBufferComponent>(cube, ubc);

            //finally, we can try adding a material component
            MaterialComponent matc;
            matc.textureID = resource.requestTexture("resources/textures/green_texture.png");
            ecsWorld.addComponent<MaterialComponent>(cube, matc);

            // and a transform component 
            TransformComponent tc;
            ecsWorld.addComponent<TransformComponent>(cube, tc);

            // a second cube, woooh
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

            //lets also add a camera :D
            Entity camera = ecsWorld.createEntity();
            CameraComponent cc;
            ecsWorld.addComponent(camera, cc);

            TransformComponent ct;
            ct.position = glm::vec3(0.0f, 0.0f, 10.0f);
            ecsWorld.addComponent(camera, ct);

            // add the camera system to the ecs world
            ecsWorld.addSystem<CameraSystem>();

            // add the rotation system to the ecs world for testing
            ecsWorld.addSystem<TransformAnimationSystem>();

            // this system will update the uniform buffer object
            ecsWorld.addSystem<UniformDataSystem>();
        }

        void onUpdate(float dt) override
        {
            ecsWorld.update(dt);
        }
};