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
#include "IGtsGraphicsModule.hpp"

class VulkanGraphics : public IGtsGraphicsModule
{
public:
    GraphicsConfig config;
    // the window manager contains the window we render to
    std::unique_ptr<WindowManager> windowManager;

    // the vulkan context, containing all the major vulkan objects needed to actually produce something
    std::unique_ptr<VulkanContext> vContext;

    // the renderer, responsible for the core drawframe function
    std::unique_ptr<ForwardRenderer> renderer;

    // window event propagation, but a lot more simple than before
    GtsEvent<int, int>& onResize() override { return windowManager->onResize(); }
    GtsEvent<GtsKeyEvent>& onKeyPressed() override { return windowManager->onKeyPressed(); }

    // renderer event propagation
    GtsEvent<int, uint32_t>& onFrameEnded() override { return renderer->onFrameEnded; }


    VulkanGraphics(const GraphicsConfig& config): config(config)
    {
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
        windowManager = std::make_unique<WindowManager>(wmConfig);
    }

    // create a concrete Vulkan Context object, accessible with an accessheet on a global basis
    void createContext()
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        VulkanContextConfig vcConfig;
        vcConfig.enableValidationLayers   = config.enableValidationLayers;
        vcConfig.vulkanInstanceExtensions = std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
        vcConfig.outputWindowPtr          = windowManager->getOutputWindow();
        vcConfig.presentModePreference    = config.presentModePreference;
        vContext = std::make_unique<VulkanContext>(vcConfig);
        vcsheet::SetContext(vContext.get());
    }

    // create the forward renderer
    void createRenderer()
    {
        RendererConfig rConfig;
        renderer = std::make_unique<ForwardRenderer>(rConfig);
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

    void pollWindowEvents() override
    {
        windowManager->getOutputWindow()->pollEvents();
    }

    void shutdown() override
    {
        vkDeviceWaitIdle(vcsheet::getDevice());
        cleanup();
    }

    bool isWindowOpen() const override
    {
        return !windowManager->getOutputWindow()->shouldClose();
    }

    float getAspectRatio() const override
    {
        return windowManager->getOutputWindow()->getAspectRatio();
    }

    void getViewportSize(int& width, int& height) const override
    {
        windowManager->getOutputWindow()->getSize(width, height);
    }

    IResourceProvider* getResourceProvider() override
    {
        return renderer->getResourceSystem();
    }
};
