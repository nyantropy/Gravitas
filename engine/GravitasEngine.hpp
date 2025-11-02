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

#include "SceneManager.hpp"
#include "DefaultScene.hpp"

class GravitasEngine 
{
    private:
        // a global engine timer
        std::unique_ptr<Timer> timer;

        // graphics module related structures
        std::unique_ptr<Graphics> graphics;
        std::unique_ptr<RenderSystem> renderSystem;

        // the scene manager
        // the whole world is a stage after all
        std::unique_ptr<SceneManager> sceneManager;

        void createSystems()
        {
            // make a render system
            renderSystem = std::make_unique<RenderSystem>();
        }
    public:
        GravitasEngine()
        {
            // create the graphics module
            GraphicsConfig config;
            config.outputWindowHeight = 800;
            config.outputWindowWidth = 800;
            config.outputWindowTitle = "Engine Test";
            graphics = std::make_unique<Graphics>(config);

            sceneManager = std::make_unique<SceneManager>();

            createSystems();
        }

        ~GravitasEngine()
        {
            if(sceneManager) sceneManager.reset();
            if(renderSystem) renderSystem.reset();
            if(graphics) graphics.reset();
            if(timer) timer.reset();
        }

        Timer* getTimer()
        {
            return timer.get();
        }

        void createDebugScene()
        {
            sceneManager->registerScene("default", std::make_unique<DefaultScene>());
            sceneManager->setActiveScene("default");
            sceneManager->getActiveScene()->onLoad(*graphics->getResourceProvider());
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
            sceneManager->getActiveScene()->onUpdate(dt);
        }

        // render call
        void render(float dt)
        {
            graphics->renderFrame(dt, renderSystem->buildRenderList(sceneManager->getActiveScene()->getWorld()));  
        }
};