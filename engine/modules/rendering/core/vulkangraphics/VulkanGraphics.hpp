#pragma once

#include "GraphicsConstants.h"

// window manager include
#include "WindowManager.hpp"
#include "WindowManagerConfig.h"

// Vulkan Context include
#include "VulkanContext.hpp"
#include "VulkanContextConfig.h"

// output window includes
#include "OutputWindowConfig.h"
#include "GLFWOutputWindow.hpp"

// renderer includes
#include "Renderer.hpp"
#include "RendererConfig.h"
#include "ForwardRenderer.hpp"

#include "ECSWorld.hpp"
#include "vcsheet.h"

#include "Entity.h"
#include "MeshManager.hpp"

#include "RenderCommandExtractor.hpp"
#include "TransformComponent.h"

#include "GraphicsConfig.h"
#include "GtsPlatformEventBus.hpp"
#include "SubscriptionToken.hpp"
#include "GtsEventTypes.h"
#include "IGtsGraphicsModule.hpp"

#include <algorithm>
#include <iostream>

class VulkanGraphics : public IGtsGraphicsModule
{
public:
    GraphicsConfig config;
    GtsPlatformEventBus eventBus;
    // the window manager contains the window we render to
    std::unique_ptr<WindowManager> windowManager;

    // the vulkan context, containing all the major vulkan objects needed to actually produce something
    std::unique_ptr<VulkanContext> vContext;

    // the renderer, responsible for the core drawframe function
    std::unique_ptr<ForwardRenderer> renderer;
    SubscriptionToken resizeEventToken;
    bool swapchainRecreatePending = false;

    PresentModePreference resolvePresentModePreference() const
    {
        return config.window.vsync
            ? PresentModePreference::Fifo
            : config.presentModePreference;
    }

    GtsPlatformEventBus& getEventBus() override { return eventBus; }

    VulkanGraphics(const GraphicsConfig& config): config(config)
    {
        if (this->config.renderWidth == 0)
            this->config.renderWidth = static_cast<uint32_t>(std::max(1, this->config.window.width));
        if (this->config.renderHeight == 0)
            this->config.renderHeight = static_cast<uint32_t>(std::max(1, this->config.window.height));

        if (!config.headless)
            createWindow();
        createContext();
        createRenderer();
        resizeEventToken = eventBus.subscribe<GtsWindowResizeEvent>(
            [this](const GtsWindowResizeEvent& event)
            {
                if (event.width > 0 && event.height > 0 && windowManager)
                {
                    if (windowManager->getOutputWindow()->getWindowMode() == WindowMode::Windowed)
                    {
                        this->config.window.width = event.width;
                        this->config.window.height = event.height;
                        this->config.renderWidth = static_cast<uint32_t>(event.width);
                        this->config.renderHeight = static_cast<uint32_t>(event.height);
                        if (renderer)
                            renderer->setRenderResolution(this->config.renderWidth, this->config.renderHeight);
                    }
                }
                swapchainRecreatePending = true;
            });
    }

    // create a new output window wrapped in a window manager
    void createWindow()
    {
        WindowManagerConfig wmConfig;
        wmConfig.windowBackend          = WindowBackend::GLFW;
        wmConfig.windowWidth            = config.window.width;
        wmConfig.windowHeight           = config.window.height;
        wmConfig.windowTitle            = config.window.title;
        wmConfig.enableValidationLayers = config.enableValidationLayers;
        wmConfig.windowMode             = config.window.windowMode;
        wmConfig.monitorIndex           = config.window.monitorIndex;
        wmConfig.monitorName            = config.window.monitorName;
        windowManager = std::make_unique<WindowManager>(wmConfig, eventBus);
        config.window.monitorIndex = windowManager->getOutputWindow()->getConfig().monitorIndex;
        config.window.monitorName = windowManager->getOutputWindow()->getConfig().monitorName;
    }

    // create a concrete Vulkan Context object, accessible with an accessheet on a global basis
    void createContext()
    {
        VulkanContextConfig vcConfig;
        vcConfig.enableValidationLayers   = config.enableValidationLayers;
        vcConfig.headless                 = config.headless;
        vcConfig.enableSurfaceSupport     = !config.headless;
        vcConfig.renderWidth              = config.renderWidth;
        vcConfig.renderHeight             = config.renderHeight;
        vcConfig.presentModePreference    = resolvePresentModePreference();

        if (!config.headless)
        {
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
            vcConfig.vulkanInstanceExtensions =
                std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
            vcConfig.outputWindowPtr = windowManager->getOutputWindow();
        }

        vContext = std::make_unique<VulkanContext>(vcConfig);
        vcsheet::SetContext(vContext.get());

        if (config.headless)
        {
            std::cout << "Running in headless mode" << std::endl;
            std::cout << "Headless output resolution: "
                      << config.renderWidth << "x" << config.renderHeight << std::endl;
        }
        else
        {
            std::cout << "Running in windowed mode" << std::endl;
        }
    }

    // create the forward renderer
    void createRenderer()
    {
        RendererConfig rConfig;
        rConfig.headless = config.headless;
        rConfig.internalScalingEnabled =
            !config.headless && config.window.windowMode == WindowMode::BorderlessFullscreen;
        rConfig.renderWidth = config.renderWidth;
        rConfig.renderHeight = config.renderHeight;
        rConfig.maxScreenshotsPerRun = config.maxScreenshotsPerRun;
        rConfig.minSecondsBetweenScreenshots = config.minSecondsBetweenScreenshots;
        renderer = std::make_unique<ForwardRenderer>(rConfig, eventBus);
        if (config.headless)
        {
            std::cout << "Headless offscreen format: "
                      << static_cast<int>(vcsheet::getFrameOutputFormat()) << std::endl;
        }
    }

    void cleanup()
    {
        renderer.reset();
        vContext.reset();
        windowManager.reset();
    }

    // whenever the graphics module renders a frame, we poll the window events, and direct
    // the draw call to the renderer
    void renderFrame(float dt, const std::vector<RenderCommand>& renderList,
                     const std::vector<ObjectUploadCommand>& objectUploads,
                     const std::vector<CameraUploadCommand>& cameraUploads,
                     const ParticleFrameData& particleData,
                     const UiCommandBuffer& uiBuffer,
                     const GtsFrameStats& stats) override
    {
        if (swapchainRecreatePending && !recreateSwapchainResourcesIfPossible())
            return;

        renderer->renderFrame(dt, renderList, objectUploads, cameraUploads, particleData, uiBuffer, stats);

        if (renderer->consumeFrameOutputRecreateRequested())
        {
            swapchainRecreatePending = true;
            recreateSwapchainResourcesIfPossible();
        }
    }

    void toggleDebugOverlay() override
    {
        renderer->toggleDebugOverlay();
    }

    void cycleDebugOverlayPage() override
    {
        renderer->cycleDebugOverlayPage();
    }

    void requestScreenshot() override
    {
        renderer->requestScreenshot();
    }

    void waitIdle() override
    {
        vkDeviceWaitIdle(vcsheet::getDevice());
    }

    void pollWindowEvents() override
    {
        if (windowManager)
            windowManager->getOutputWindow()->pollEvents();
    }

    void shutdown() override
    {
        vkDeviceWaitIdle(vcsheet::getDevice());
        cleanup();
    }

    bool isWindowOpen() const override
    {
        if (!windowManager)
            return true;
        return !windowManager->getOutputWindow()->shouldClose();
    }

    float getAspectRatio() const override
    {
        if (!windowManager)
            return static_cast<float>(config.renderWidth) / static_cast<float>(config.renderHeight);
        if (config.window.windowMode == WindowMode::BorderlessFullscreen)
            return static_cast<float>(config.renderWidth) / static_cast<float>(config.renderHeight);
        return windowManager->getOutputWindow()->getAspectRatio();
    }

    void getViewportSize(int& width, int& height) const override
    {
        if (!windowManager)
        {
            width = static_cast<int>(config.renderWidth);
            height = static_cast<int>(config.renderHeight);
            return;
        }
        windowManager->getOutputWindow()->getSize(width, height);
    }

    RuntimeGraphicsSettings getRuntimeGraphicsSettings() const override
    {
        return RuntimeGraphicsSettings{
            static_cast<int>(config.renderWidth),
            static_cast<int>(config.renderHeight),
            config.window.windowMode,
            config.window.vsync,
            config.presentModePreference,
            config.maxFrameRate,
            config.window.monitorIndex,
            config.window.monitorName
        };
    }

    std::vector<GraphicsMonitorInfo> getAvailableMonitors() const override
    {
        if (!windowManager)
            return {};
        return windowManager->getOutputWindow()->getAvailableMonitors();
    }

    bool applyRuntimeGraphicsSettings(const RuntimeGraphicsSettings& settings) override
    {
        if (settings.width <= 0 || settings.height <= 0)
            return false;

        config.renderWidth = static_cast<uint32_t>(settings.width);
        config.renderHeight = static_cast<uint32_t>(settings.height);
        config.window.width = settings.width;
        config.window.height = settings.height;
        config.window.windowMode = settings.windowMode;
        config.window.monitorIndex = std::max(0, settings.monitorIndex);
        config.window.monitorName = settings.monitorName;
        config.window.vsync = settings.vsync;
        config.presentModePreference = settings.presentModePreference;
        config.maxFrameRate = std::max(0, settings.maxFrameRate);

        if (!config.headless && windowManager)
        {
            OutputWindow* window = windowManager->getOutputWindow();
            const auto monitors = window->getAvailableMonitors();
            if (!monitors.empty())
            {
                config.window.monitorIndex = std::clamp(
                    config.window.monitorIndex,
                    0,
                    static_cast<int>(monitors.size()) - 1);
            }
            window->applyWindowSettings(config.window.width,
                                        config.window.height,
                                        config.window.windowMode,
                                        config.window.monitorIndex,
                                        config.window.monitorName);
            config.window.monitorIndex = window->getConfig().monitorIndex;
            config.window.monitorName = window->getConfig().monitorName;
        }

        if (renderer)
        {
            renderer->setInternalScalingEnabled(
                !config.headless && config.window.windowMode == WindowMode::BorderlessFullscreen);
            renderer->setRenderResolution(config.renderWidth, config.renderHeight);
        }
        swapchainRecreatePending = true;
        recreateSwapchainResourcesIfPossible();
        return true;
    }

    IResourceProvider* getResourceProvider() override
    {
        return renderer->getResourceSystem();
    }

private:
    bool currentWindowExtentValid() const
    {
        int width = 0;
        int height = 0;
        getViewportSize(width, height);
        return width > 0 && height > 0;
    }

    bool recreateSwapchainResourcesIfPossible()
    {
        if (config.headless)
        {
            swapchainRecreatePending = false;
            return true;
        }

        if (!swapchainRecreatePending)
            return true;
        if (!currentWindowExtentValid() || vContext == nullptr || renderer == nullptr)
            return false;

        vkDeviceWaitIdle(vcsheet::getDevice());
        renderer->releaseFrameResources();
        vContext->recreateSwapChain(resolvePresentModePreference());
        renderer->rebuildFrameResources();
        swapchainRecreatePending = false;
        return true;
    }
};
