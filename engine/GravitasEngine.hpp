#pragma once

#include <memory>
#include <iostream>
#include <chrono>

#include "EngineConfig.h"
#include "Timer.hpp"
#include "TimeContext.h"

#include "GtsPlatform.h"
#include "GtsGameLoop.h"

#include "GtsExtractorContext.h"
#include "RenderCommandExtractor.hpp"
#include "UiSystem.h"
#include "SceneManager.hpp"

#include "GtsCommand.h"
#include "GtsCommandBuffer.h"
#include "GtsFrameStats.h"
#include "RenderGpuSystem.hpp"

class GravitasEngine
{
    private:
        EngineConfig engineConfig;

        // OS-facing subsystems: graphics, windowing, input
        GtsPlatform platform;

        // Fixed-timestep accumulator
        GtsGameLoop gameLoop;

        // a global engine timer
        std::unique_ptr<Timer> timer;

        // used to extract all render commands from the currently active ecs world
        std::unique_ptr<RenderCommandExtractor> renderCommandExtractor;

        // retained engine UI system — owned here, exposed to scenes via SceneContext::ui
        std::unique_ptr<UiSystem> uiSystem;

        // the scene manager
        // the whole world is a stage after all
        std::unique_ptr<SceneManager> sceneManager;

        // the scene context and the time context should both be members of the engine, as they get updated
        // constantly and need to not run out of scope
        SceneContext sceneContext;
        TimeContext  timeContext;

        // the engine also has its own command buffer, which can be filled by lower level architectures, and will lead
        // to the engine executing pre defined functions like pausing, or changing the scene, etc..
        GtsCommandBuffer engineCommands;

        void createSceneContext()
        {
            int viewportWidth = 1;
            int viewportHeight = 1;
            platform.getViewportSize(viewportWidth, viewportHeight);

            sceneContext.resources         = platform.getResourceProvider();
            sceneContext.inputSource       = platform.getInputSource();   // pause-gated
            sceneContext.actions           = platform.getActionManager(); // always live
            sceneContext.time              = &timeContext;
            sceneContext.engineCommands    = &engineCommands;
            sceneContext.windowAspectRatio = platform.getAspectRatio();
            sceneContext.windowPixelWidth  = viewportWidth;
            sceneContext.windowPixelHeight = viewportHeight;
            sceneContext.extractor         = renderCommandExtractor.get();
            sceneContext.ui                = uiSystem.get();
            sceneContext.physics           = nullptr;
        }

        // its only a render system now, maybe this will move later
        void createECSExtractors()
        {
            renderCommandExtractor = std::make_unique<RenderCommandExtractor>(engineConfig.frustumCullingEnabled);
            uiSystem               = std::make_unique<UiSystem>(platform.getResourceProvider());
        }

        // render call
        void render(float dt)
        {
            GtsScene* activeScene = sceneManager->getActiveScene();
            auto& world = activeScene->getWorld();
            int viewportWidth = 1;
            int viewportHeight = 1;
            platform.getViewportSize(viewportWidth, viewportHeight);
            sceneContext.windowAspectRatio = platform.getAspectRatio();
            sceneContext.windowPixelWidth = viewportWidth;
            sceneContext.windowPixelHeight = viewportHeight;

            GtsFrameStats stats;
            stats.fps            = (dt > 0.0f) ? 1.0f / dt : 0.0f;
            stats.frameTimeMs    = dt * 1000.0f;
            stats.visibleObjects = static_cast<uint32_t>(renderCommandExtractor->getLastVisibleRenderables());
            stats.totalObjects   = static_cast<uint32_t>(renderCommandExtractor->getLastTotalRenderables());
            stats.controllerSystemCount = static_cast<uint32_t>(world.getControllerSystemCount());
            stats.simulationSystemCount = static_cast<uint32_t>(world.getSimulationSystemCount());
            const auto renderMetrics = RenderGpuSystem::getLastMetrics();
            stats.renderGpuUpdatedCount = renderMetrics.updatedRenderables;
            stats.renderGpuCpuMs        = renderMetrics.cpuTimeMs;
            // triangleCount is filled in by SceneRenderStage during execute
            activeScene->populateFrameStats(stats);

            GtsExtractorContext extractCtx{world, sceneContext.windowAspectRatio};

            const auto extractStart = std::chrono::steady_clock::now();
            auto renderList = renderCommandExtractor->extract(extractCtx);
            const auto extractEnd = std::chrono::steady_clock::now();

            auto uiBuffer   = uiSystem->extractCommands(sceneContext.windowPixelWidth,
                                                       sceneContext.windowPixelHeight);

            const auto extractorMetrics = renderCommandExtractor->getLastMetrics();
            const auto uiMetrics = uiSystem->getLastMetrics();
            stats.renderExtractCpuMs = std::chrono::duration<float, std::milli>(extractEnd - extractStart).count();
            stats.uiLayoutCpuMs      = uiMetrics.layoutMs;
            stats.uiVisualCpuMs      = uiMetrics.visualMs;
            stats.uiCommandCpuMs     = uiMetrics.commandBuildMs;
            stats.uiNodeCount        = uiMetrics.nodeCount;
            stats.uiPrimitiveCount   = uiMetrics.primitiveCount;
            stats.uiCommandCount     = uiMetrics.commandCount;
            stats.totalObjects       = extractorMetrics.totalRenderables;
            stats.visibleObjects     = extractorMetrics.visibleRenderables;

            const auto submitStart = std::chrono::steady_clock::now();
            platform.getGraphics()->renderFrame(dt, renderList, uiBuffer, stats);
            const auto submitEnd = std::chrono::steady_clock::now();
            stats.renderSubmitCpuMs =
                std::chrono::duration<float, std::milli>(submitEnd - submitStart).count();
        }

        // command callback from lower level architectures
        void applyCommands()
        {
            for (auto& cmd : engineCommands.commands)
            {
                switch (cmd.type)
                {
                    case GtsCommand::Type::TogglePause:
                        gameLoop.paused = !gameLoop.paused;
                        std::cout << (gameLoop.paused ? "Paused" : "Resumed") << std::endl;
                        break;
                    case GtsCommand::Type::LoadScene:
                    case GtsCommand::Type::ChangeScene:
                        uiSystem->clear();
                        sceneContext.physics = nullptr;
                        sceneManager->setActiveScene(cmd.stringArg);
                        sceneManager->getActiveScene()->onLoad(sceneContext, cmd.transitionData.get());
                        break;
                    case GtsCommand::Type::Quit:
                        break;
                }
            }

            engineCommands.commands.clear();
        }

    public:
        explicit GravitasEngine(EngineConfig config = EngineConfig{})
            : engineConfig(std::move(config))
            , platform(engineConfig)
        {
            gameLoop.init(engineConfig);
            sceneManager = std::make_unique<SceneManager>();
            createECSExtractors();
            createSceneContext();
        }

        ~GravitasEngine() = default;

        void registerScene(std::string name, std::unique_ptr<GtsScene> scene)
        {
            sceneManager->registerScene(name, std::move(scene));
        }

        void setActiveScene(std::string name,
                            std::shared_ptr<GtsSceneTransitionData> data = nullptr)
        {
            uiSystem->clear();
            sceneContext.physics = nullptr;
            sceneManager->setActiveScene(name);
            sceneManager->getActiveScene()->onLoad(sceneContext, data.get());
        }

        void start()
        {
            timer = std::make_unique<Timer>();

            // order in the function calls matters a lot, especially for the input system!!!
            while (platform.isWindowOpen())
            {
                // tick the engine timer
                float realDt = timer->tick();
                timeContext.unscaledDeltaTime = realDt;

                // snapshot previous frame, poll OS events, then derive action states
                platform.beginFrame();

                // engine-level actions (pause, quit) — run before tick advance.
                auto* actions = platform.getActionManager();
                if (actions->isActionPressed(GtsAction::CloseApplication)) break;
                if (actions->isActionPressed(GtsAction::TogglePause))
                    gameLoop.paused = !gameLoop.paused;
                if (actions->isActionPressed(GtsAction::DebugLayerToggle))
                    platform.toggleDebugOverlay();

                platform.setSimulationPaused(gameLoop.paused);

                // fixed timestep simulation ticks
                int ticks             = gameLoop.advance(realDt);
                timeContext.deltaTime = gameLoop.simulationDt();
                timeContext.frame     = timer->getFrameCount();

                for (int i = 0; i < ticks; ++i)
                    sceneManager->getActiveScene()->onUpdateSimulation(sceneContext);

                // controller systems and rendering run every frame
                sceneManager->getActiveScene()->onUpdateControllers(sceneContext);
                render(realDt);
                applyCommands();
            }

            // shutdown graphics module after we close the window
            platform.shutdown();
        }
};
