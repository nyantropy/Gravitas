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
#include "IGtsGraphicsModule.hpp"

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

    GtsPlatformEventBus& getEventBus() override { return eventBus; }

    VulkanGraphics(const GraphicsConfig& config): config(config)
    {
        if (!config.headless)
            createWindow();
        createContext();
        createRenderer();
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
        windowManager = std::make_unique<WindowManager>(wmConfig, eventBus);
    }

    // create a concrete Vulkan Context object, accessible with an accessheet on a global basis
    void createContext()
    {
        VulkanContextConfig vcConfig;
        vcConfig.enableValidationLayers   = config.enableValidationLayers;
        vcConfig.headless                 = config.headless;
        vcConfig.enableSurfaceSupport     = !config.headless;
        vcConfig.renderWidth              = config.window.width;
        vcConfig.renderHeight             = config.window.height;
        vcConfig.presentModePreference    = config.presentModePreference;

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
                      << config.window.width << "x" << config.window.height << std::endl;
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
        rConfig.renderWidth = config.window.width;
        rConfig.renderHeight = config.window.height;
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
                     const UiCommandBuffer& uiBuffer,
                     const GtsFrameStats& stats) override
    {
        renderer->renderFrame(dt, renderList, uiBuffer, stats);
    }

    void toggleDebugOverlay() override
    {
        renderer->toggleDebugOverlay();
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
            return static_cast<float>(config.window.width) / static_cast<float>(config.window.height);
        return windowManager->getOutputWindow()->getAspectRatio();
    }

    void getViewportSize(int& width, int& height) const override
    {
        if (!windowManager)
        {
            width = static_cast<int>(config.window.width);
            height = static_cast<int>(config.window.height);
            return;
        }
        windowManager->getOutputWindow()->getSize(width, height);
    }

    IResourceProvider* getResourceProvider() override
    {
        return renderer->getResourceSystem();
    }
};
