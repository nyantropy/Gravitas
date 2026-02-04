#pragma once

#include <memory>

#include "ECSWorld.hpp"
#include "Timer.hpp"
#include "Graphics.hpp"
#include "RenderCommandExtractor.hpp"

#include "SceneManager.hpp"
#include "DefaultScene.hpp"

#include "InputManager.hpp"
#include "gtsinput.h"

class GravitasEngine 
{
    private:
        // a global engine timer
        std::unique_ptr<Timer> timer;

        // the global input manager
        std::unique_ptr<InputManager> inputManager;

        // graphics module related structures
        std::unique_ptr<Graphics> graphics;
        std::unique_ptr<RenderCommandExtractor> renderSystem;

        // the scene manager
        // the whole world is a stage after all
        std::unique_ptr<SceneManager> sceneManager;

        // its only a render system now, maybe this will move later
        void createSystems()
        {
            renderSystem = std::make_unique<RenderCommandExtractor>();
        }

        // create Manager classes
        void createManagers()
        {
            sceneManager = std::make_unique<SceneManager>();
            inputManager = std::make_unique<InputManager>();
            gtsinput::SetInputManager(inputManager.get());
        }

        // create the Graphics class
        void createGraphicsModule()
        {
            GraphicsConfig config;
            config.outputWindowHeight = 800;
            config.outputWindowWidth = 800;
            config.outputWindowTitle = "Engine Test";
            graphics = std::make_unique<Graphics>(config);
        }
    public:
        GravitasEngine()
        {
            createManagers();
            createGraphicsModule();
            createSystems();
        }

        ~GravitasEngine()
        {
            if(inputManager) inputManager.reset();
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

            // order in the function calls matters a lot, especially for the input system!!!
            while(graphics->isWindowOpen())
            {
                // tick the engine timer
                float dt = timer->tick();

                // let the input manager catch the window keydown event
                inputManager->beginFrame();
                graphics->pollWindowEvents();

                // update scene and entities, render frame
                update(dt);
                render(dt);
            }

            // shutdown graphics module after we close the window
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
            graphics->renderFrame(dt, renderSystem->extractRenderList(sceneManager->getActiveScene()->getWorld()));  
        }
};