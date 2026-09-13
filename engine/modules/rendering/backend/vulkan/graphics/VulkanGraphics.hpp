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

#include "VulkanBackendContext.h"

#include "GraphicsConfig.h"
#include "GtsPlatformEventBus.hpp"
#include "SubscriptionToken.hpp"
#include "GtsEventTypes.h"
#include "IGtsGraphicsModule.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

class VulkanGraphics : public IGtsGraphicsModule
{
private:
    const GraphicsConfig config;
    GraphicsSettings requested;
    Extent2D renderExtent;

public:
    GtsPlatformEventBus eventBus;
    // the window manager contains the window we render to
    std::unique_ptr<WindowManager> windowManager;

    // the vulkan context, containing all the major vulkan objects needed to actually produce something
    std::unique_ptr<VulkanContext> vContext;
    std::unique_ptr<VulkanBackendContext> backendContext;

    // the renderer, responsible for the core drawframe function
    std::unique_ptr<ForwardRenderer> renderer;
    SubscriptionToken resizeEventToken;
    bool swapchainRecreatePending = false;

    PresentModePreference resolvePresentModePreference() const
    {
        return requested.presentation.mode;
    }

    GtsPlatformEventBus& getEventBus() override { return eventBus; }

    VulkanGraphics(const GraphicsConfig& config): config(config), requested(config.settings)
    {
        const std::string error = validateGraphicsSettings(requested);
        if (!error.empty())
            throw std::invalid_argument(error);

        if (!config.startup.headless)
            createWindow();
        Extent2D output{static_cast<uint32_t>(requested.window.width),
                        static_cast<uint32_t>(requested.window.height)};
        if (windowManager)
        {
            int width = 0;
            int height = 0;
            windowManager->getOutputWindow()->getSize(width, height);
            output = {static_cast<uint32_t>(std::max(1, width)),
                      static_cast<uint32_t>(std::max(1, height))};
        }
        const auto initialExtent = resolveRenderExtent(requested.rendering.resolution, output);
        if (!initialExtent)
            throw std::invalid_argument("Render resolution cannot be resolved for this output");
        renderExtent = *initialExtent;
        createContext();
        if (!supportsRenderExtent(renderExtent))
            throw std::invalid_argument("Render resolution exceeds device limits");
        createRenderer();
        resizeEventToken = eventBus.subscribe<GtsWindowResizeEvent>(
            [this](const GtsWindowResizeEvent& event)
            {
                swapchainRecreatePending = true;
            });
    }

    // create a new output window wrapped in a window manager
    void createWindow()
    {
        WindowManagerConfig wmConfig;
        wmConfig.windowBackend          = WindowBackend::GLFW;
        wmConfig.window.settings = requested.window;
        wmConfig.window.title = config.windowTitle;
        windowManager = std::make_unique<WindowManager>(wmConfig, eventBus);
    }

    // create a concrete Vulkan Context object and backend dependency context
    void createContext()
    {
        VulkanContextConfig vcConfig;
        vcConfig.enableValidationLayers   = config.startup.enableValidationLayers;
        vcConfig.headless                 = config.startup.headless;
        vcConfig.enableSurfaceSupport     = !config.startup.headless;
        vcConfig.renderWidth              = renderExtent.width;
        vcConfig.renderHeight             = renderExtent.height;
        vcConfig.presentModePreference    = resolvePresentModePreference();

        if (!config.startup.headless)
        {
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
            vcConfig.vulkanInstanceExtensions =
                std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
            vcConfig.outputWindowPtr = windowManager->getOutputWindow();
        }

        vContext = std::make_unique<VulkanContext>(vcConfig);
        backendContext = std::make_unique<VulkanBackendContext>(*vContext);

        if (config.startup.headless)
        {
            std::cout << "Running in headless mode" << std::endl;
            std::cout << "Headless output resolution: "
                      << renderExtent.width << "x" << renderExtent.height << std::endl;
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
        rConfig.headless = config.startup.headless;
        rConfig.internalScalingEnabled =
            !config.startup.headless && requested.rendering.resolution.mode != RenderResolutionMode::MatchOutput;
        rConfig.renderWidth = renderExtent.width;
        rConfig.renderHeight = renderExtent.height;
        rConfig.maxScreenshotsPerRun = config.screenshots.maxScreenshotsPerRun;
        rConfig.minSecondsBetweenScreenshots = config.screenshots.minSecondsBetweenScreenshots;
        rConfig.enableGpuTimestamps = config.startup.enableGpuTimestamps;
        renderer = std::make_unique<ForwardRenderer>(rConfig, *backendContext, eventBus);
        if (config.startup.headless)
        {
            std::cout << "Headless offscreen format: "
                      << static_cast<int>(backendContext->frameOutputFormat()) << std::endl;
        }
    }

    void cleanup()
    {
        renderer.reset();
        backendContext.reset();
        vContext.reset();
        windowManager.reset();
    }

    // whenever the graphics module renders a frame, we poll the window events, and direct
    // the draw call to the renderer
    void renderFrame(float dt, const std::vector<RenderCommand>& renderList,
                     const MaterialFrameData& materialFrameData,
                     const std::vector<ObjectUploadCommand>& objectUploads,
                     const std::vector<CameraUploadCommand>& cameraUploads,
                     const ParticleFrameData& particleData,
                     const RenderViewportRect& sceneViewport,
                     const UiCommandBuffer& uiBuffer,
                     const EditorPreviewRenderData& editorPreview,
                     const GtsFrameStats& stats, const SkinnedFrameData& skinnedData = {}) override
    {
        if (swapchainRecreatePending && !recreateSwapchainResourcesIfPossible())
            return;

        renderer->renderFrame(dt,
                              renderList,
                              materialFrameData,
                              objectUploads,
                              cameraUploads,
                              particleData,
                              sceneViewport,
                              uiBuffer,
                              editorPreview,
                              stats, skinnedData);

        if (renderer->consumeFrameOutputRecreateRequested())
        {
            swapchainRecreatePending = true;
            recreateSwapchainResourcesIfPossible();
        }
    }

    GtsFrameStats getLastFrameStats() const override
    {
        return renderer ? renderer->getLastFrameStats() : GtsFrameStats{};
    }

    texture_id_type ensureEditorPreviewTarget(uint32_t width, uint32_t height) override
    {
        return renderer ? renderer->ensureEditorPreviewTarget(width, height) : 0;
    }

    void releaseEditorPreviewTarget() override
    {
        if (renderer)
            renderer->releaseEditorPreviewTarget();
    }

    void toggleDebugOverlay() override
    {
        renderer->toggleDebugOverlay();
    }

    void cycleDebugOverlayPage() override
    {
        renderer->cycleDebugOverlayPage();
    }

    void requestScreenshot(const std::string& outputDirectory = {}) override
    {
        renderer->requestScreenshot(outputDirectory);
    }

    void waitIdle() override
    {
        vkDeviceWaitIdle(backendContext->device());
    }

    void pollWindowEvents() override
    {
        if (windowManager)
            windowManager->getOutputWindow()->pollEvents();
    }

    void shutdown() override
    {
        vkDeviceWaitIdle(backendContext->device());
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
        const auto extent = renderer->getSceneRenderExtent();
        return static_cast<float>(extent.width) / static_cast<float>(extent.height);
    }

    void getViewportSize(int& width, int& height) const override
    {
        if (!windowManager)
        {
            width = static_cast<int>(renderExtent.width);
            height = static_cast<int>(renderExtent.height);
            return;
        }
        windowManager->getOutputWindow()->getSize(width, height);
    }

    GraphicsSettings getRequestedGraphicsSettings() const override
    {
        return requested;
    }

    GraphicsRuntimeState getGraphicsRuntimeState() const override
    {
        GraphicsRuntimeState state;
        if (windowManager)
            state.window = windowManager->getOutputWindow()->getRuntimeSettings();
        const auto output = vContext->getFrameOutputExtent();
        const auto scene = renderer->getSceneRenderExtent();
        state.outputExtent = {output.width, output.height};
        state.renderExtent = {scene.width, scene.height};
        if (!config.startup.headless)
        {
            switch (vContext->getFrameOutputPresentMode())
            {
                case VK_PRESENT_MODE_IMMEDIATE_KHR: state.presentMode = PresentModePreference::Immediate; break;
                case VK_PRESENT_MODE_MAILBOX_KHR: state.presentMode = PresentModePreference::Mailbox; break;
                default: state.presentMode = PresentModePreference::Fifo; break;
            }
        }
        state.pending = swapchainRecreatePending;
        return state;
    }

    std::vector<GraphicsMonitorInfo> getAvailableMonitors() const override
    {
        if (!windowManager)
            return {};
        return windowManager->getOutputWindow()->getAvailableMonitors();
    }

    GraphicsSettingsApplyResult applyGraphicsSettings(const GraphicsSettings& settings) override
    {
        const std::string error = validateGraphicsSettings(settings);
        if (!error.empty())
            return {GraphicsSettingsApplyStatus::Rejected, error};

        const auto output = vContext->getFrameOutputExtent();
        const auto extent = resolveRenderExtent(settings.rendering.resolution, {output.width, output.height});
        if (!extent)
            return {GraphicsSettingsApplyStatus::Rejected, "Render resolution cannot be resolved"};
        if (!supportsRenderExtent(*extent))
            return {GraphicsSettingsApplyStatus::Rejected, "Render resolution exceeds device limits"};
        if (config.startup.headless && *extent != Extent2D{output.width, output.height})
            return {GraphicsSettingsApplyStatus::Rejected, "Headless output resizing requires restart"};

        const bool windowChanged = requested.window != settings.window;
        const bool resourcesChanged = windowChanged || *extent != renderExtent
            || requested.presentation.mode != settings.presentation.mode
            || requested.rendering.resolution.mode != settings.rendering.resolution.mode;
        requested = settings;

        if (windowChanged && windowManager)
        {
            OutputWindow* window = windowManager->getOutputWindow();
            window->applyWindowSettings(requested.window.width,
                                        requested.window.height,
                                        requested.window.windowMode,
                                        requested.window.monitorIndex,
                                        requested.window.monitorName);
        }

        swapchainRecreatePending = swapchainRecreatePending || resourcesChanged;
        if (!recreateSwapchainResourcesIfPossible())
            return {GraphicsSettingsApplyStatus::Pending, "Waiting for a drawable output"};
        return {GraphicsSettingsApplyStatus::Applied, {}};
    }

    IResourceProvider* getResourceProvider() override
    {
        return renderer->getResourceSystem();
    }

private:
    bool supportsRenderExtent(Extent2D extent) const
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(backendContext->physicalDevice(), &properties);
        return extent.width <= std::min(properties.limits.maxImageDimension2D, properties.limits.maxFramebufferWidth)
            && extent.height <= std::min(properties.limits.maxImageDimension2D, properties.limits.maxFramebufferHeight);
    }

    bool currentWindowExtentValid() const
    {
        int width = 0;
        int height = 0;
        getViewportSize(width, height);
        return width > 0 && height > 0;
    }

    bool recreateSwapchainResourcesIfPossible()
    {
        if (config.startup.headless)
        {
            swapchainRecreatePending = false;
            return true;
        }

        if (!swapchainRecreatePending)
            return true;
        if (!currentWindowExtentValid() || vContext == nullptr || renderer == nullptr)
            return false;

        vkDeviceWaitIdle(backendContext->device());
        renderer->releaseFrameResources();
        vContext->recreateSwapChain(resolvePresentModePreference());
        const auto output = vContext->getFrameOutputExtent();
        const auto extent = resolveRenderExtent(requested.rendering.resolution, {output.width, output.height});
        if (!extent)
            throw std::runtime_error("Cannot resolve render extent after output resize");
        renderExtent = *extent;
        renderer->setInternalScalingEnabled(requested.rendering.resolution.mode != RenderResolutionMode::MatchOutput);
        renderer->setRenderResolution(renderExtent.width, renderExtent.height);
        renderer->rebuildFrameResources();
        swapchainRecreatePending = false;
        return true;
    }
};
