#pragma once

#include <memory>

#include "EngineConfig.h"
#include "ECSWorld.hpp"
#include "Timer.hpp"
#include "TimeContext.h"
#include "Graphics.hpp"
#include "RenderCommandExtractor.hpp"

#include "SceneManager.hpp"

#include "InputManager.hpp"
#include "InputActionManager.hpp"
#include "PauseFilteredInputSource.hpp"
#include "GtsAction.h"

#include "GtsCommand.h"
#include "GtsCommandBuffer.h"

class GravitasEngine
{
    private:
        EngineConfig engineConfig;

        // a global engine timer
        std::unique_ptr<Timer> timer;

        // the global input manager and its action abstraction layer
        std::unique_ptr<InputManager>                  inputManager;
        std::unique_ptr<InputActionManager<GtsAction>> actionManager;

        // Wraps inputManager and gates all key queries to false while the simulation
        // is paused.  Exposed as sceneContext.inputSource so simulation-coupled
        // controller systems (e.g. TetrisInputSystem) see no input during pause.
        // ctx.actions (engine's InputActionManager) is updated from the raw inputManager
        // directly, so camera and UI systems are unaffected.
        PauseFilteredInputSource filteredInputSource;

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

        void bindDefaultActions()
        {
            actionManager->bind(GtsAction::TogglePause, GtsKey::X);
        }

        void createSceneContext()
        {
            sceneContext.resources         = graphics->getResourceProvider();
            sceneContext.inputSource       = &filteredInputSource;   // pause-gated
            sceneContext.actions           = actionManager.get();    // always live
            sceneContext.time              = &timeContext;
            sceneContext.engineCommands    = &engineCommands;
            sceneContext.windowAspectRatio = graphics->windowManager->getOutputWindow()->getAspectRatio();
        }

        // its only a render system now, maybe this will move later
        void createECSExtractors()
        {
            renderCommandExtractor = std::make_unique<RenderCommandExtractor>();
        }

        // create Manager classes
        void createManagers()
        {
            sceneManager  = std::make_unique<SceneManager>();
            inputManager  = std::make_unique<InputManager>();
            actionManager = std::make_unique<InputActionManager<GtsAction>>();
            filteredInputSource.setSource(*inputManager);
        }

        // create the Graphics class and wire the InputManager into the window
        void createGraphicsModule()
        {
            graphics = std::make_unique<Graphics>(engineConfig.graphics);
            graphics->windowManager->getOutputWindow()->setInputManager(inputManager.get());
        }
    public:
        explicit GravitasEngine(EngineConfig config = EngineConfig{}) : engineConfig(std::move(config))
        {
            createManagers();
            createGraphicsModule();
            createECSExtractors();
            createSceneContext();
            bindDefaultActions();
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
                timeContext.unscaledDeltaTime = realDt;

                // snapshot previous frame, poll OS events, then derive action states
                inputManager->beginFrame();
                graphics->pollWindowEvents();
                actionManager->update(*inputManager);

                // engine-level action handling — pause toggle runs before deltaTime is
                // computed so that the simulation sees dt=0 on the exact pause frame.
                if (actionManager->isActionPressed(GtsAction::TogglePause))
                    simulationPaused = !simulationPaused;

                // deltaTime uses the updated simulationPaused state.
                // The filtered input source is also updated so that simulation-coupled
                // controller systems see no input while the simulation is paused.
                timeContext.deltaTime = simulationPaused ? 0.0f : realDt * timeContext.timeScale;
                timeContext.frame = timer->getFrameCount();
                filteredInputSource.setSimulationPaused(simulationPaused);

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
                    case GtsCommand::Type::TogglePause:
                        simulationPaused = !simulationPaused;
                        std::cout << (simulationPaused ? "Paused" : "Resumed") << std::endl;
                        break;
                    case GtsCommand::Type::LoadScene:
                        sceneManager->setActiveScene(cmd.stringArg);
                        break;
                }
            }

            engineCommands.commands.clear();
        }
};