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
#include "RenderExtractionSnapshotBuilder.hpp"
#include "UiSystem.h"
#include "SceneManager.hpp"

#include "GtsCommand.h"
#include "GtsCommandBuffer.h"
#include "GtsFrameStats.h"
#include "RenderGpuSystem.hpp"

#include "EcsSimulationContext.hpp"
#include "EcsControllerContext.hpp"

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
        RenderExtractionSnapshotBuilder         renderSnapshotBuilder;

        // retained engine UI system
        std::unique_ptr<UiSystem> uiSystem;

        // the scene manager
        std::unique_ptr<SceneManager> sceneManager;

        // Per-frame state — persistent fields for building EcsControllerContext
        TimeContext     timeContext;
        GtsCommandBuffer engineCommands;
        float            windowAspectRatio  = 1.0f;
        int              windowPixelWidth   = 1;
        int              windowPixelHeight  = 1;
        bool             uiEnabled          = true;

        bool engineRunning = true;

        // Build an EcsControllerContext from the engine's current frame state.
        // physics is sourced from the active scene so it is always up to date.
        EcsControllerContext buildControllerContext(ECSWorld& world)
        {
            EcsControllerContext ctx{world};
            ctx.resources         = platform.getResourceProvider();
            ctx.input             = platform.getInputBindingRegistry();
            ctx.time              = &timeContext;
            ctx.engineCommands    = &engineCommands;
            ctx.extractor         = renderCommandExtractor.get();
            ctx.ui                = uiSystem.get();
            ctx.physics           = sceneManager->getActiveScene()->getPhysicsModule();
            ctx.windowAspectRatio = windowAspectRatio;
            ctx.windowPixelWidth  = static_cast<float>(windowPixelWidth);
            ctx.windowPixelHeight = static_cast<float>(windowPixelHeight);
            return ctx;
        }

        // render call
        void render(float dt)
        {
            GtsScene* activeScene = sceneManager->getActiveScene();
            auto& world = activeScene->getWorld();
            platform.getViewportSize(windowPixelWidth, windowPixelHeight);
            windowAspectRatio = platform.getAspectRatio();

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

            const RenderExtractionSnapshot& renderSnapshot = renderSnapshotBuilder.build(world);
            const auto extractStart = std::chrono::steady_clock::now();
            const auto& renderList = renderCommandExtractor->extract(renderSnapshot);
            const auto extractEnd = std::chrono::steady_clock::now();

            UiCommandBuffer uiBuffer;
            if (uiEnabled)
            {
                uiSystem->setEnabled(true);
                uiBuffer = uiSystem->extractCommands(windowPixelWidth, windowPixelHeight);
            }
            else
            {
                uiSystem->setEnabled(false);
            }

            const auto extractorMetrics = renderCommandExtractor->getLastMetrics();
            const auto uiMetrics = uiSystem->getLastMetrics();
            stats.renderExtractCpuMs = std::chrono::duration<float, std::milli>(extractEnd - extractStart).count();
            stats.renderExtractSortCpuMs = extractorMetrics.sortCpuMs;
            stats.uiLayoutCpuMs      = uiMetrics.layoutMs;
            stats.uiVisualCpuMs      = uiMetrics.visualMs;
            stats.uiCommandCpuMs     = uiMetrics.commandBuildMs;
            stats.uiNodeCount        = uiMetrics.nodeCount;
            stats.uiPrimitiveCount   = uiMetrics.primitiveCount;
            stats.uiCommandCount     = uiMetrics.commandCount;
            stats.totalObjects       = extractorMetrics.totalRenderables;
            stats.visibleObjects     = extractorMetrics.visibleRenderables;
            stats.renderCommandVisitedCount = extractorMetrics.visitedRenderables;
            stats.renderCommandTotalCount   = extractorMetrics.cachedCommands;
            stats.renderCommandUpdatedCount = extractorMetrics.updatedCommands;
            stats.renderCommandSortedCount  = extractorMetrics.sortedThisFrame ? 1u : 0u;

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
                    case GtsCommand::Type::Screenshot:
                        platform.getGraphics()->requestScreenshot();
                        break;
                    case GtsCommand::Type::LoadScene:
                    case GtsCommand::Type::ChangeScene:
                    {
                        platform.waitForGraphicsIdle();
                        uiSystem->clear();
                        platform.getInputBindingRegistry()->clearContextStack();
                        sceneManager->setActiveScene(cmd.stringArg);
                        uiSystem->setEnabled(uiEnabled);
                        ECSWorld& world = sceneManager->getActiveScene()->getWorld();
                        EcsControllerContext loadCtx = buildControllerContext(world);
                        sceneManager->getActiveScene()->onLoad(loadCtx, cmd.transitionData.get());
                        break;
                    }
                    case GtsCommand::Type::Quit:
                        engineRunning = false;
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
            renderCommandExtractor = std::make_unique<RenderCommandExtractor>(engineConfig.frustumCullingEnabled);
            uiSystem               = std::make_unique<UiSystem>(platform.getResourceProvider());
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
            platform.getInputBindingRegistry()->clearContextStack();
            sceneManager->setActiveScene(name);
            uiSystem->setEnabled(uiEnabled);
            ECSWorld& world = sceneManager->getActiveScene()->getWorld();
            EcsControllerContext loadCtx = buildControllerContext(world);
            sceneManager->getActiveScene()->onLoad(loadCtx, data.get());
        }

        void start()
        {
            timer = std::make_unique<Timer>();
            engineRunning = true;

            // order in the function calls matters a lot, especially for the input system!!!
            while (engineRunning && platform.isWindowOpen())
            {
                // tick the engine timer
                float realDt = timer->tick();
                timeContext.unscaledDeltaTime = realDt;

                // snapshot previous frame, poll OS events, then derive action states
                platform.beginFrame();

                // engine-level actions (pause, quit) — run before tick advance.
                auto* input = platform.getInputBindingRegistry();
                if (input->isPressed("engine.close"))
                    break;
                if (input->isPressed("engine.pause"))
                    gameLoop.paused = !gameLoop.paused;
                if (input->isPressed("engine.debug_overlay"))
                    platform.toggleDebugOverlay();
                if (input->isPressed("engine.screenshot"))
                    platform.getGraphics()->requestScreenshot();
                if (input->isPressed("engine.toggle_ui"))
                {
                    uiEnabled = !uiEnabled;
                    uiSystem->setEnabled(uiEnabled);
                    std::cout << (uiEnabled ? "UI: ON" : "UI: OFF") << std::endl;
                }

                input->setPaused(gameLoop.paused);

                // Fixed timestep simulation ticks
                int ticks             = gameLoop.advance(realDt);
                timeContext.deltaTime = gameLoop.simulationDt();
                timeContext.frame     = timer->getFrameCount();

                ECSWorld& world = sceneManager->getActiveScene()->getWorld();

                for (int i = 0; i < ticks; ++i)
                {
                    EcsSimulationContext simCtx{ world, gameLoop.simulationDt(), input };
                    sceneManager->getActiveScene()->onUpdateSimulation(simCtx);
                }

                // Controller systems and rendering run every frame
                {
                    EcsControllerContext ctrlCtx = buildControllerContext(world);
                    sceneManager->getActiveScene()->onUpdateControllers(ctrlCtx);
                }

                world.flushEvents();
                render(realDt);
                applyCommands();
            }

            // shutdown graphics module after we close the window
            platform.shutdown();
        }
};
