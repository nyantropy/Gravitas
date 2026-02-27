#pragma once

#include <memory>

#include "ECSWorld.hpp"
#include "Timer.hpp"
#include "TimeContext.h"
#include "Graphics.hpp"
#include "RenderCommandExtractor.hpp"

#include "SceneManager.hpp"

#include "InputManager.hpp"

#include "GtsCommand.h"
#include "GtsCommandBuffer.h"

class GravitasEngine 
{
    private:
        // a global engine timer
        std::unique_ptr<Timer> timer;

        // the global input manager
        std::unique_ptr<InputManager> inputManager;

        // graphics module related structures
        std::unique_ptr<Graphics> graphics;

        // used to extract all render commands from the currently active ecs world
        std::unique_ptr<RenderCommandExtractor> renderCommandExtractor;

        // the scene manager
        // the whole world is a stage after all
        std::unique_ptr<SceneManager> sceneManager;

        // the scene context and the time context should both be members of the engine, as they get updated
        // constantly and need to not run out of scope
        SceneContext sceneContext;
        TimeContext timeContext;

        // the engine also has its own command buffer, which can be filled by lower level architectures, and will lead
        // to the engine executing pre defined functions like pausing, or changing the scene, etc..
        GtsCommandBuffer engineCommands;

        bool simulationPaused = false;

        void createSceneContext()
        {
            sceneContext.resources = graphics->getResourceProvider();
            sceneContext.input = inputManager.get();
            sceneContext.time = &timeContext;
            sceneContext.engineCommands = &engineCommands;
        }

        // its only a render system now, maybe this will move later
        void createECSExtractors()
        {
            renderCommandExtractor = std::make_unique<RenderCommandExtractor>();
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
            createECSExtractors();
            createSceneContext();
        }

        ~GravitasEngine()
        {
            if(inputManager) inputManager.reset();
            if(sceneManager) sceneManager.reset();
            if(renderCommandExtractor) renderCommandExtractor.reset();
            if(graphics) graphics.reset();
            if(timer) timer.reset();
        }

        Timer* getTimer()
        {
            return timer.get();
        }

        void registerScene(std::string name, std::unique_ptr<GtsScene> scene)
        {
            sceneManager->registerScene(name, std::move(scene));
        }

        void setActiveScene(std::string name)
        {
            sceneManager->setActiveScene(name);
            sceneManager->getActiveScene()->onLoad(sceneContext);
        }

        void start()
        {
            timer = std::make_unique<Timer>();

            // order in the function calls matters a lot, especially for the input system!!!
            while(graphics->isWindowOpen())
            {
                // tick the engine timer
                float realDt = timer->tick();

                // update the context(s)
                timeContext.unscaledDeltaTime = realDt;
                timeContext.deltaTime = simulationPaused ? 0.0f : realDt * timeContext.timeScale;
                timeContext.frame = timer->getFrameCount();

                // let the input manager catch the window keydown event
                inputManager->beginFrame();
                graphics->pollWindowEvents();

                // update scene and entities, render frame
                // and also apply engine commands, if there are any in the queue
                update();
                render(realDt);
                applyCommands();
            }

            // shutdown graphics module after we close the window
            graphics->shutdown();
        }

        // update call
        void update()
        {
            sceneManager->getActiveScene()->onUpdate(sceneContext);
        }

        // render call
        void render(float dt)
        {
            graphics->renderFrame(dt, renderCommandExtractor->extractRenderList(sceneManager->getActiveScene()->getWorld()));  
        }

        // command callback from lower level architectures
        void applyCommands()
        {
            for (auto& cmd : engineCommands.commands)
            {
                switch (cmd.type)
                {
                    case GtsCommand::Type::Pause:
                        simulationPaused = true;
                        std::cout << "Paused" << std::endl;
                        break;
                    case GtsCommand::Type::Resume:
                        simulationPaused = false;
                        std::cout << "Resumed" << std::endl;
                        break;
                    case GtsCommand::Type::LoadScene:
                        sceneManager->setActiveScene(cmd.stringArg);
                        break;
                }
            }

            engineCommands.commands.clear();
        }
};