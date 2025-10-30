#pragma once

#include <memory>

#include "ECSWorld.hpp"
#include "Timer.hpp"
#include "Graphics.hpp"
#include "RenderSystem.hpp"

#include "CameraComponent.h"
#include "CameraSystem.hpp"

#include "TransformAnimationSystem.hpp"
#include "UniformDataSystem.hpp"

class GravitasEngine 
{
    private:
        // ecs world and the global timer are owned by the engine
        std::unique_ptr<ECSWorld> ecsWorld;
        std::unique_ptr<Timer> timer;

        // graphics module related structures
        std::unique_ptr<Graphics> graphics;
        std::unique_ptr<RenderSystem> renderSystem;

        void createSystems()
        {
            // make a render system
            renderSystem = std::make_unique<RenderSystem>();

            // add the camera system to the ecs world
            ecsWorld->addSystem<CameraSystem>();

            // add the rotation system to the ecs world for testing
            ecsWorld->addSystem<TransformAnimationSystem>();

            // this system will update the uniform buffer object
            ecsWorld->addSystem<UniformDataSystem>();



        }
    public:
        GravitasEngine()
        {
            // create a new ecs world
            ecsWorld = std::make_unique<ECSWorld>();

            // create the graphics module
            GraphicsConfig config;
            config.outputWindowHeight = 800;
            config.outputWindowWidth = 800;
            config.outputWindowTitle = "Engine Test";
            graphics = std::make_unique<Graphics>(config);

            createSystems();
            createEmptyScene();
        }

        ~GravitasEngine()
        {
            if(renderSystem) renderSystem.reset();
            if(graphics) graphics.reset();
            if(timer) timer.reset();
            if(ecsWorld) ecsWorld.reset();
        }

        ECSWorld* getECSWorld()
        {
            return ecsWorld.get();
        }

        Timer* getTimer()
        {
            return timer.get();
        }

        void start()
        {
            timer = std::make_unique<Timer>();

            while(graphics->isWindowOpen())
            {
                float dt = timer->tick();
                update(dt);
                render(dt);
            }

            graphics->shutdown();
        }

        void stop()
        {

        }

        // update call
        void update(float dt)
        {
            ecsWorld->update(dt);
        }

        // render call
        void render(float dt)
        {
            graphics->renderFrame(dt, renderSystem->buildRenderList(*ecsWorld.get()));  
        }

        void createEmptyScene()
        {
            //lets make a cube
            Entity cube = ecsWorld->createEntity();

            //and add a mesh component to it
            MeshComponent mc;
            mc.meshID = graphics->getResourceProvider()->requestMesh("resources/models/cube.obj");
            ecsWorld->addComponent<MeshComponent>(cube, mc);

            //now we can try adding a uniform buffer component as well
            UniformBufferComponent ubc;
            ubc.uniformID = graphics->getResourceProvider()->requestUniformBuffer();
            ecsWorld->addComponent<UniformBufferComponent>(cube, ubc);

            //finally, we can try adding a material component
            MaterialComponent matc;
            matc.textureID = graphics->getResourceProvider()->requestTexture("resources/textures/green_texture.png");
            ecsWorld->addComponent<MaterialComponent>(cube, matc);

            // and a transform component 
            TransformComponent tc;
            ecsWorld->addComponent<TransformComponent>(cube, tc);

            // a second cube, woooh
            Entity cube2 = ecsWorld->createEntity();

            MeshComponent mc2;
            mc2.meshID = graphics->getResourceProvider()->requestMesh("resources/models/cube.obj");
            ecsWorld->addComponent<MeshComponent>(cube2, mc2);

            UniformBufferComponent ubc2;
            ubc2.uniformID = graphics->getResourceProvider()->requestUniformBuffer();
            ecsWorld->addComponent<UniformBufferComponent>(cube2, ubc2);

            MaterialComponent matc2;
            matc2.textureID = graphics->getResourceProvider()->requestTexture("resources/textures/blue_texture.png");
            ecsWorld->addComponent<MaterialComponent>(cube2, matc2);

            TransformComponent tc2;
            tc2.position = glm::vec3(2.0f, 2.0f, 2.0f);
            ecsWorld->addComponent<TransformComponent>(cube2, tc2);

            //lets also add a camera :D
            Entity camera = ecsWorld->createEntity();
            CameraComponent cc;
            ecsWorld->addComponent(camera, cc);

            TransformComponent ct;
            ct.position = glm::vec3(0.0f, 0.0f, 10.0f);
            ecsWorld->addComponent(camera, ct);

        }
};