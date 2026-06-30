#pragma once

#include <memory>
#include <iostream>
#include <chrono>
#include <thread>
#include <variant>
#include <vector>

#include "EngineConfig.h"
#include "Timer.hpp"
#include "TimeContext.h"

#include "GtsPlatform.h"
#include "GtsGameLoop.h"

#include "SceneManager.hpp"

#include "GtsCommand.h"
#include "GtsCommandBuffer.h"
#include "EngineServiceRegistry.h"
#include "IEngineModule.h"
#include "GraphicsBackendInstaller.h"
#include "GraphicsBackendRegistry.h"
#include "ProfileAccumulator.h"
#include "RenderingRuntime.h"
#include "EngineToolRuntime.hpp"

#include "EcsSimulationContext.hpp"
#include "EcsControllerContext.hpp"

class GravitasEngine
{
    private:
    EngineConfig engineConfig;

    // registered graphics backends installed before platform creation
    gts::rendering::GraphicsBackendRegistry graphicsBackends;

    // OS-facing subsystems: graphics, windowing, input
    GtsPlatform platform;

    // Fixed-timestep accumulator
    GtsGameLoop gameLoop;

    // a global engine timer
    std::unique_ptr<Timer> timer;

    // installed engine-level service registry and module lifecycle hooks
    EngineServiceRegistry serviceRegistry;
    std::vector<IEngineModule*> engineModules;

    // renderer runtime integration
    std::unique_ptr<gts::rendering::RenderingRuntime> renderingRuntime;

    // the scene manager
    std::unique_ptr<SceneManager> sceneManager;

    // global development tooling runtime
    std::unique_ptr<gts::tools::EngineToolRuntime> toolRuntime;

    // Per-frame state — persistent fields for building EcsControllerContext
    TimeContext      timeContext;
    GtsCommandBuffer engineCommands;
    float            windowAspectRatio = 1.0f;
    int              windowPixelWidth  = 1;
    int              windowPixelHeight = 1;
    bool             uiEnabled         = true;
    int              maxFrameRate      = 0;

    bool engineRunning = true;

    // Per-frame CPU timing buckets accumulated in start(), consumed in render()
    float              lastSimCpuMs   = 0.0f;
    float              lastCtrlCpuMs  = 0.0f;
    float              lastFrameCpuMs = 0.0f;
    ProfileAccumulator profiler;

    static gts::rendering::GraphicsBackendRegistry createDefaultGraphicsBackendRegistry()
    {
        gts::rendering::GraphicsBackendRegistry registry;
        gts::rendering::installDefaultGraphicsBackends(registry);
        return registry;
    }

    void installEngineModule(IEngineModule& module)
    {
        engineModules.push_back(&module);
        module.registerServices(serviceRegistry);
    }

    void uninstallEngineModules()
    {
        for (auto it = engineModules.rbegin(); it != engineModules.rend(); ++it)
            (*it)->unregisterServices(serviceRegistry);
        engineModules.clear();
        serviceRegistry.clear();
    }

    void refreshWindowMetrics()
    {
        platform.getViewportSize(windowPixelWidth, windowPixelHeight);
        windowAspectRatio = platform.getAspectRatio();
    }

    // Build an EcsControllerContext from the engine's current frame state.
    // physics is sourced from the active scene so it is always up to date.
    EcsControllerContext buildControllerContext(ECSWorld& world)
    {
        GtsScene* activeScene = sceneManager->getActiveScene();

        EcsControllerContext ctx{world};
        ctx.resources         = renderingRuntime->resources();
        ctx.input             = platform.getInputBindingRegistry();
        ctx.time              = &timeContext;
        ctx.engineCommands    = &engineCommands;
        ctx.ui                = renderingRuntime->ui();
        ctx.physics           = activeScene == nullptr ? nullptr : activeScene->getPhysicsModule();
        ctx.registeredScenes  = &sceneManager->getRegisteredScenes();
        ctx.activeSceneName   = &sceneManager->getActiveSceneName();
        ctx.windowAspectRatio = windowAspectRatio;
        ctx.windowPixelWidth  = static_cast<float>(windowPixelWidth);
        ctx.windowPixelHeight = static_cast<float>(windowPixelHeight);
        ctx.sceneViewportPixelX = 0.0f;
        ctx.sceneViewportPixelY = 0.0f;
        ctx.sceneViewportPixelWidth = static_cast<float>(windowPixelWidth);
        ctx.sceneViewportPixelHeight = static_cast<float>(windowPixelHeight);
        ctx.sceneViewportAspectRatio = windowAspectRatio;
        return ctx;
    }

    void unloadActiveScene()
    {
        GtsScene* activeScene = sceneManager->getActiveScene();
        if (activeScene == nullptr)
            return;

        ECSWorld&            world     = activeScene->getWorld();
        EcsControllerContext unloadCtx = buildControllerContext(world);
        for (auto it = engineModules.rbegin(); it != engineModules.rend(); ++it)
            (*it)->beforeSceneUnload(*activeScene, unloadCtx);

        activeScene->unload(unloadCtx);
        sceneManager->clearActiveScene();

        for (auto it = engineModules.rbegin(); it != engineModules.rend(); ++it)
            (*it)->afterSceneUnload(serviceRegistry);
    }

    void loadScene(std::string name, const GtsSceneTransitionData* data)
    {
        renderingRuntime->clearUi();
        platform.getInputBindingRegistry()->clearContextStack();
        sceneManager->setActiveScene(std::move(name));
        renderingRuntime->setUiEnabled(uiEnabled);

        GtsScene*            activeScene = sceneManager->getActiveScene();
        ECSWorld&            world       = activeScene->getWorld();
        EcsControllerContext loadCtx     = buildControllerContext(world);
        for (IEngineModule* module : engineModules)
            module->beforeSceneLoad(*activeScene, loadCtx);

        toolRuntime->prepare(loadCtx);
        renderingRuntime->applySceneViewportMetrics(loadCtx, windowPixelWidth, windowPixelHeight);
        activeScene->onLoad(loadCtx, data);

        for (IEngineModule* module : engineModules)
            module->afterSceneLoad(*activeScene, loadCtx);
    }

    void switchScene(std::string name, const GtsSceneTransitionData* data)
    {
        platform.waitForGraphicsIdle();
        unloadActiveScene();
        loadScene(std::move(name), data);
    }

    // render call
    void render(float dt)
    {
        GtsScene* activeScene = sceneManager->getActiveScene();
        refreshWindowMetrics();
        renderingRuntime->renderFrame(
            dt,
            *activeScene,
            timeContext,
            lastSimCpuMs,
            lastCtrlCpuMs,
            lastFrameCpuMs,
            toolRuntime == nullptr ? 0u : toolRuntime->controllerSystemCount(),
            windowPixelWidth,
            windowPixelHeight,
            profiler);
    }

    // command callback from lower level architectures
    void applyGraphicsSettingsCommand(const RuntimeGraphicsSettings& settings)
    {
        if (platform.applyRuntimeGraphicsSettings(settings))
        {
            engineConfig.graphics.renderWidth           = static_cast<uint32_t>(settings.width);
            engineConfig.graphics.renderHeight          = static_cast<uint32_t>(settings.height);
            engineConfig.graphics.window.width          = settings.width;
            engineConfig.graphics.window.height         = settings.height;
            engineConfig.graphics.window.windowMode     = settings.windowMode;
            engineConfig.graphics.window.monitorIndex   = settings.monitorIndex;
            engineConfig.graphics.window.vsync          = settings.vsync;
            engineConfig.graphics.presentModePreference = settings.presentModePreference;
            engineConfig.graphics.maxFrameRate          = settings.maxFrameRate;
            maxFrameRate                                = settings.maxFrameRate;
        }
    }

    void applyPendingRenderCommands()
    {
        for (auto it = engineCommands.commands.begin(); it != engineCommands.commands.end();)
        {
            if (auto* cmd = std::get_if<GtsExtensionCommand>(&*it))
            {
                if (applyPendingRenderingCommand(*cmd))
                    it = engineCommands.commands.erase(it);
                else
                    ++it;
            }
            else
            {
                ++it;
            }
        }
    }

    bool applyPendingRenderingCommand(const GtsExtensionCommand& command)
    {
        return renderingRuntime->applyExtensionCommand(command);
    }

    void applyCommands()
    {
        for (auto& cmd : engineCommands.commands)
        {
            if (std::holds_alternative<GtsTogglePauseCommand>(cmd))
            {
                gameLoop.paused = !gameLoop.paused;
                std::cout << (gameLoop.paused ? "Paused" : "Resumed") << std::endl;
            }
            else if (std::holds_alternative<GtsScreenshotCommand>(cmd))
            {
                platform.getGraphics()->requestScreenshot();
            }
            else if (auto* changeScene = std::get_if<GtsChangeSceneCommand>(&cmd))
            {
                switchScene(changeScene->name, changeScene->transitionData.get());
            }
            else if (std::holds_alternative<GtsQuitCommand>(cmd))
            {
                engineRunning = false;
            }
        }

        engineCommands.commands.clear();
    }

    public:
    explicit GravitasEngine(EngineConfig config = EngineConfig{})
        : engineConfig(std::move(config))
        , graphicsBackends(createDefaultGraphicsBackendRegistry())
        , platform(engineConfig, graphicsBackends)
    {
        gameLoop.init(engineConfig);
        maxFrameRate   = engineConfig.graphics.maxFrameRate;
        sceneManager   = std::make_unique<SceneManager>();
        renderingRuntime =
            std::make_unique<gts::rendering::RenderingRuntime>(
                engineConfig.frustumCullingEnabled,
                *platform.getGraphics(),
                [this](const RuntimeGraphicsSettings& settings)
                {
                    applyGraphicsSettingsCommand(settings);
                });
        installEngineModule(*renderingRuntime);
        toolRuntime = std::make_unique<gts::tools::EngineToolRuntime>();
    }

    ~GravitasEngine() = default;

    void registerScene(std::string name, SceneManager::SceneFactory factory, RegisteredSceneInfo info = {})
    {
        sceneManager->registerScene(std::move(name), std::move(factory), std::move(info));
    }

    void setActiveScene(std::string name, std::unique_ptr<GtsSceneTransitionData> data = nullptr)
    {
        platform.waitForGraphicsIdle();
        unloadActiveScene();
        loadScene(std::move(name), data.get());
    }

    void start()
    {
        timer         = std::make_unique<Timer>();
        engineRunning = true;

        // order in the function calls matters a lot, especially for the input system!!!
        while (engineRunning && platform.isWindowOpen())
        {
            const auto loopStart = std::chrono::steady_clock::now();

            // tick the engine timer
            float realDt                  = timer->tick();
            timeContext.unscaledDeltaTime = realDt;

            // snapshot previous frame, poll OS events, then derive action states
            platform.beginFrame();
            refreshWindowMetrics();

            // engine-level actions (pause, quit) — run before tick advance.
            auto* input = platform.getInputBindingRegistry();
            if (input->isPressed("engine.close"))
                break;
            if (input->isPressed("engine.pause"))
                gameLoop.paused = !gameLoop.paused;
            if (input->isPressed("engine.debug_overlay"))
                platform.toggleDebugOverlay();
            if (input->isPressed("engine.debug_overlay_page"))
                platform.cycleDebugOverlayPage();
            if (input->isPressed("engine.screenshot"))
                platform.getGraphics()->requestScreenshot();
            if (input->isPressed("engine.toggle_ui"))
            {
                uiEnabled = !uiEnabled;
                renderingRuntime->setUiEnabled(uiEnabled);
                std::cout << (uiEnabled ? "UI: ON" : "UI: OFF") << std::endl;
            }

            input->setPaused(gameLoop.paused);

            // Fixed timestep simulation ticks — timed for CPU profiling
            int ticks                   = gameLoop.advance(realDt);
            timeContext.deltaTime       = gameLoop.simulationDt();
            timeContext.frame           = timer->getFrameCount();
            timeContext.simulationAlpha = gameLoop.alpha();

            ECSWorld& world = sceneManager->getActiveScene()->getWorld();
            renderingRuntime->dispatchUiInput(input, windowPixelWidth, windowPixelHeight, timeContext.frame);

            {
                const auto simStart = std::chrono::steady_clock::now();
                for (int i = 0; i < ticks; ++i)
                {
                    EcsSimulationContext simCtx{world, gameLoop.simulationDt(), input};
                    sceneManager->getActiveScene()->onUpdateSimulation(simCtx);
                    input->finishSimulationTick();
                }
                const auto simEnd = std::chrono::steady_clock::now();
                lastSimCpuMs      = std::chrono::duration<float, std::milli>(simEnd - simStart).count();
            }

            // Controller systems — timed for CPU profiling
            {
                const auto           ctrlStart = std::chrono::steady_clock::now();
                EcsControllerContext ctrlCtx   = buildControllerContext(world);
                toolRuntime->prepare(ctrlCtx);
                EcsControllerContext sceneCtx = buildControllerContext(world);
                renderingRuntime->applySceneViewportMetrics(sceneCtx, windowPixelWidth, windowPixelHeight);
                sceneManager->getActiveScene()->onUpdateControllers(sceneCtx);
                toolRuntime->update(ctrlCtx);
                const auto ctrlEnd = std::chrono::steady_clock::now();
                lastCtrlCpuMs      = std::chrono::duration<float, std::milli>(ctrlEnd - ctrlStart).count();
            }

            world.flushEvents();
            applyPendingRenderCommands();
            render(realDt);
            applyCommands();

            const auto preSleepLoopEnd = std::chrono::steady_clock::now();
            if (maxFrameRate > 0)
            {
                const auto targetFrameDuration = std::chrono::duration<float>(1.0f / static_cast<float>(maxFrameRate));
                const auto elapsed             = preSleepLoopEnd - loopStart;
                if (elapsed < targetFrameDuration)
                {
                    std::this_thread::sleep_for(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(targetFrameDuration - elapsed));
                }
            }

            // Store total loop time for next frame's frameCpuMs stat
            const auto loopEnd = std::chrono::steady_clock::now();
            lastFrameCpuMs     = std::chrono::duration<float, std::milli>(loopEnd - loopStart).count();
        }

        platform.waitForGraphicsIdle();
        unloadActiveScene();
        if (toolRuntime != nullptr)
        {
            toolRuntime->shutdown();
            toolRuntime.reset();
        }
        renderingRuntime->clearUi();
        uninstallEngineModules();

        // shutdown graphics module after we close the window
        platform.shutdown();
    }
};
