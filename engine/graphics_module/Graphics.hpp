#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/hash.hpp>

extern "C" {
    #ifdef HAVE_AV_CONFIG_H
    #undef HAVE_AV_CONFIG_H
    #endif

    #include <libavutil/imgutils.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libavformat/avformat.h>
    #include <libavutil/opt.h>
    #include <libavutil/timestamp.h>
}

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>

#include "GraphicsConstants.h"
#include "Vertex.h"
#include "UniformBufferObject.h"

#include "VulkanShader.hpp"
#include "VulkanPipeline.hpp"

#include "VulkanTexture.hpp"
#include "GtsModelLoader.hpp"
#include "GtsCamera.hpp"

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

#include "VulkanPipelineConfig.h"

#include "ECSWorld.hpp"
#include "vcsheet.h"

#include "Entity.h"
#include "MeshComponent.h"
#include "MeshManager.hpp"

#include "UniformBufferComponent.h"
#include "UniformBufferManager.hpp"

#include "DescriptorSetManager.hpp"
#include "dssheet.h"

#include "TextureManager.hpp"
#include "MaterialComponent.h"

#include "RenderSystem.hpp"
#include "TransformComponent.h"

#include "ResourceSystem.hpp"
#include "GraphicsConfig.h"

#include "GravitasEngine.hpp"

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
//     auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
//     if (func != nullptr) {
//         return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
//     } else {
//         return VK_ERROR_EXTENSION_NOT_PRESENT;
//     }
// }

// void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
//     auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
//     if (func != nullptr) {
//         func(instance, debugMessenger, pAllocator);
//     }
// }

class Graphics 
{
public:
    GraphicsConfig config;
    // the window manager contains the window we render to
    std::unique_ptr<WindowManager> windowManager;

    // the vulkan context, containing all the major vulkan objects needed to actually produce something
    std::unique_ptr<VulkanContext> vContext;

    // the renderer, responsible for the core drawframe function
    std::unique_ptr<ForwardRenderer> renderer;

    // the render system, responsible for creating rendercommands, which will be issued to the renderer
    std::unique_ptr<RenderSystem> renderSystem;

    GtsCamera* vcamera;

    // window event propagation, but a lot more simple than before
    GtsEvent<int, int>& onResize() { return windowManager->onResize(); }
    GtsEvent<int, int, int, int>& onKeyPressed() { return windowManager->onKeyPressed(); }

    // renderer event propagation
    GtsEvent<int, uint32_t>& onFrameEnded() { return renderer->onFrameEnded; }


    Graphics(const GraphicsConfig& config): config(config)
    {
        createWindow();
        createContext();
        createRenderer();
        vcamera = new GtsCamera(vContext.get()->getSwapChainWrapper()->getSwapChainExtent());
        renderSystem = std::make_unique<RenderSystem>();
    }

    // create a new output window wrapped in a window manager
    void createWindow()
    {
        WindowManagerConfig wmConfig;
        wmConfig.windowWidth = config.outputWindowWidth;
        wmConfig.windowHeight = config.outputWindowHeight;
        wmConfig.windowTitle = config.outputWindowTitle;
        windowManager = std::make_unique<WindowManager>(wmConfig);
    }

    // create a concrete Vulkan Context object, accessible with an accessheet on a global basis
    void createContext()
    {
        VulkanContextConfig vcConfig;
        vcConfig.enableValidationLayers = enableValidationLayers;
        vcConfig.vulkanInstanceExtensions = windowManager->getOutputWindow()->getRequiredExtensions();
        vcConfig.outputWindowPtr = windowManager->getOutputWindow();
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
        renderSystem.reset();
        delete vcamera;
        renderer.reset();
        vContext.reset();
        windowManager.reset();
    }

    // whenever the graphics module renders a frame, we poll the window events, and direct
    // the draw call to the renderer
    void renderFrame(float dt, const std::vector<RenderCommand> renderList, ECSWorld& ecsWorld) 
    {
        windowManager->getOutputWindow()->pollEvents();
        renderer->renderFrame(dt, renderList, *vcamera, ecsWorld);
    }

    void shutdown()
    {
        vkDeviceWaitIdle(vcsheet::getDevice());
        cleanup();
    }

    bool isWindowOpen()
    {
        return !windowManager->getOutputWindow()->shouldClose();
    }

    IResourceProvider* getResourceProvider()
    {
        return renderer->getResourceSystem();
    }

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};