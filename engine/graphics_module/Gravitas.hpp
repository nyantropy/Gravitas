#ifndef GRAVITAS_HPP
#define GRAVITAS_HPP

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

#include "GTSDescriptorSetManager.hpp"
#include "VulkanShader.hpp"
#include "VulkanPipeline.hpp"

#include "GtsBufferService.hpp"
#include "VulkanTexture.hpp"
#include "GtsModelLoader.hpp"
#include "GtsCamera.hpp"
#include "GtsRenderableObject.hpp"
#include "GtsSceneNode.hpp"
#include "GtsScene.hpp"
#include "GtsSceneNodeOpt.h"
#include "GtsOnSceneUpdatedEvent.hpp"
#include "GtsOnFrameEndedEvent.hpp"
#include "GtsEncoder.hpp"
#include "GtsFrameGrabber.hpp"

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
#include "VulkanFramebufferManager.hpp"
#include "VulkanFramebufferManagerConfig.h"

#include "FrameSyncObjects.hpp"

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

class Gravitas 
{
public:

    // core components
    //---------------------------------------------
    // the output window, where frames are presented
    //std::unique_ptr<OutputWindow> windowManager->getOutputWindow();

    std::unique_ptr<WindowManager> windowManager;

    // the vulkan context, containing all the major vulkan objects needed to actually produce something
    std::unique_ptr<VulkanContext> vContext;

    // the renderer, responsible for the core render loop
    std::unique_ptr<ForwardRenderer> renderer;

    VulkanPipeline* vpipeline;

    VulkanFramebufferManager* vframebuffer;
    GtsCamera* vcamera;

    GtsScene* currentScene;
    GtsSceneNode* selectedNode;

    GtsOnSceneUpdatedEvent onSceneUpdatedEvent;
    GtsOnFrameEndedEvent onFrameEndedEvent;

    GtsFrameGrabber* framegrabber;
    GtsEncoder* encoder;

    FrameSyncObjects* fso;


    ECSWorld* ecsWorld;

    MeshManager* meshManager;
    UniformBufferManager* uniformBufferManager;
    DescriptorSetManager* descriptorSetManager;
    TextureManager* textureManager;
    
    RenderSystem* renderSystem;

    // window event propagation, but a lot more simple than before
    GtsEvent<int, int>& onResize() { return windowManager->onResize(); }
    GtsEvent<int, int, int, int>& onKeyPressed() { return windowManager->onKeyPressed(); }

    void subscribeOnSceneUpdatedEvent(std::function<void()> f)
    {
        onSceneUpdatedEvent.subscribe(f);
    }

    void subscribeOnFrameEndedEvent(std::function<void(int, uint32_t)> f)
    {
        onFrameEndedEvent.subscribe(f);
    }

    void init(uint32_t width, uint32_t height, std::string title)
    {
        // create a window manager to encapsulate the output window and its events
        WindowManagerConfig wmConfig;
        wmConfig.windowWidth = width;
        wmConfig.windowHeight = height;
        wmConfig.windowTitle = title;
        windowManager = std::make_unique<WindowManager>(wmConfig);

        // create the vulkan context, and set it as the active context for global access
        VulkanContextConfig vcConfig;
        vcConfig.enableValidationLayers = enableValidationLayers;
        vcConfig.vulkanInstanceExtensions = windowManager->getOutputWindow()->getRequiredExtensions();
        vcConfig.outputWindowPtr = windowManager->getOutputWindow();
        vContext = std::make_unique<VulkanContext>(vcConfig);
        vcsheet::SetContext(vContext.get());

        // After creating swapchain and obtaining imageCount:
        uint32_t swapchainImageCount = vContext->getSwapChainWrapper()->getSwapChainImages().size();
        imagesInFlight.assign(swapchainImageCount, VK_NULL_HANDLE);


        // create the renderer
        RendererConfig rConfig;
        rConfig.vkDevice = vContext->getDevice();
        rConfig.vkPhysicalDevice = vContext->getPhysicalDevice();
        rConfig.vkExtent = vContext->getSwapChainExtent();
        rConfig.swapChainImageFormat = vContext->getSwapChainImageFormat();
        renderer = std::make_unique<ForwardRenderer>(rConfig);

        descriptorSetManager = new DescriptorSetManager(GraphicsConstants::MAX_FRAMES_IN_FLIGHT, 1000);
        dssheet::SetManager(descriptorSetManager);

        // reworked pipeline
        VulkanPipelineConfig vpConfig;
        vpConfig.fragmentShaderPath = GraphicsConstants::F_SHADER_PATH;
        vpConfig.vertexShaderPath = GraphicsConstants::V_SHADER_PATH;
        vpConfig.vkRenderPass = renderer->getRenderPassWrapper()->getRenderPass();
        vpipeline = new VulkanPipeline(vpConfig);

        // reworked vulkan frame buffer management
        VulkanFramebufferManagerConfig vfmConfig;
        vfmConfig.attachmentImageView = renderer->getAttachmentWrapper()->getImageView();
        vfmConfig.swapchainImageViews = vContext->getSwapChainImageViews();
        vfmConfig.vkDevice = vContext->getDevice();
        vfmConfig.vkExtent = vContext->getSwapChainExtent();
        vfmConfig.vkRenderpass = renderer->getRenderPassWrapper()->getRenderPass();
        vframebuffer = new VulkanFramebufferManager(vfmConfig);

        vcamera = new GtsCamera(vContext.get()->getSwapChainWrapper()->getSwapChainExtent());
        createCommandBuffers();

        fso = new FrameSyncObjects();

        meshManager = new MeshManager();
        uniformBufferManager = new UniformBufferManager();
        textureManager = new TextureManager();
        renderSystem = new RenderSystem();
        ecsWorld = new ECSWorld();
    }

    void cleanup() 
    {
        delete ecsWorld;
        delete renderSystem;
        delete textureManager;
        delete uniformBufferManager;
        delete meshManager;

        delete fso;


        delete currentScene;
        delete vcamera;
        delete vframebuffer;
        delete vpipeline;

        delete descriptorSetManager;

        renderer.reset();
        vContext.reset();
        windowManager.reset();
    }

    void createEmptyScene()
    {
        //lets make a cube
        Entity cube = ecsWorld->createEntity();

        //and add a mesh component to it
        MeshComponent mc;
        mc.meshKey = "resources/models/cube.obj";
        mc.meshPtr = &meshManager->loadMesh(mc.meshKey);
        ecsWorld->addComponent<MeshComponent>(cube, mc);

        //now we can try adding a uniform buffer component as well
        UniformBufferComponent ubc;
        ubc.ubKey = cube.id;
        ubc.ubPtr = &uniformBufferManager->createUniformBufferResource(ubc.ubKey);
        ecsWorld->addComponent<UniformBufferComponent>(cube, ubc);

        //finally, we can try adding a material component
        MaterialComponent matc;
        matc.textureKey = "resources/textures/green_texture.png";
        matc.texturePtr = &textureManager->loadTexture(matc.textureKey);
        ecsWorld->addComponent<MaterialComponent>(cube, matc);

        // and a transform component 
        TransformComponent tc;
        ecsWorld->addComponent<TransformComponent>(cube, tc);

        std::cout << "No execution problems!" << std::endl;
    }

    void run() 
    {
        while (!windowManager->getOutputWindow()->shouldClose())
        {
            windowManager->getOutputWindow()->pollEvents();

            drawFrame();

        }

        vkDeviceWaitIdle(vContext.get()->getLogicalDeviceWrapper()->getDevice());

        cleanup();
    }

private:
    std::vector<VkCommandBuffer> commandBuffers;
    // In your renderer header/class members:
    std::vector<VkFence> imagesInFlight; // size = swapchain image count

    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    void OnFrameBufferResizeCallback(int width, int height) 
    {
        framebufferResized = true;
    }

    //we can do that once we figured out how to put everything into classes
    void recreateSwapChain() 
    {
        // int width = 0, height = 0;
        // windowManager->getOutputWindow()->getSize(width, height);

        // while (width == 0 || height == 0) 
        // {
        //     windowManager->getOutputWindow()->getSize(width, height);
        //     windowManager->getOutputWindow()->pollEvents();
        // }

        // std::cout << "swapchain recreated" << std::endl;
        // std::cout << width << std::endl;
        // std::cout << height << std::endl;

        // vkDeviceWaitIdle(vContext.get()->getLogicalDeviceWrapper()->getDevice());

        //cleanupSwapChain();

        //delete vContext.get()->getSwapChainWrapper();
        //vContext.get()->getSwapChainWrapper() = new VulkanSwapChain(windowManager->getOutputWindow(), vContext.get()->getSurfaceWrapper(), vContext.get()->getPhysicalDeviceWrapper(), vContext.get()->getLogicalDeviceWrapper());
        //createDepthResources();
        //createFramebuffers();
    }

    void createCommandBuffers() 
    {
        commandBuffers.resize(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = vContext.get()->getLogicalDeviceWrapper()->getCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(vContext.get()->getLogicalDeviceWrapper()->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderer->getRenderPassWrapper()->getRenderPass();
        renderPassInfo.framebuffer = vframebuffer->getFramebuffers()[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = vContext.get()->getSwapChainWrapper()->getSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vpipeline->getPipeline());

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float) vContext.get()->getSwapChainWrapper()->getSwapChainExtent().width;
            viewport.height = (float) vContext.get()->getSwapChainWrapper()->getSwapChainExtent().height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = vContext.get()->getSwapChainWrapper()->getSwapChainExtent();
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            
            renderSystem->update(*ecsWorld, commandBuffer, vpipeline->getPipelineLayout(), currentFrame);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    float FPS = 30.0f;
    double frameInterval = 1.0 / FPS;
    double lastFrameTime = 0.0;
    int frameCounter = 0;

    void drawFrame()
    {
        static auto previousTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();
        previousTime = currentTime;
        lastFrameTime += deltaTime;

        VkDevice device = vContext.get()->getLogicalDeviceWrapper()->getDevice();
        VkFence currentFence = fso->getInFlightFence(currentFrame);

        // Wait for this frame's fence
        vkWaitForFences(device, 1, &currentFence, VK_TRUE, UINT64_MAX);

        // Acquire swapchain image
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device,
                                                vContext.get()->getSwapChainWrapper()->getSwapChain(),
                                                UINT64_MAX,
                                                fso->getImageAvailableSemaphore(currentFrame),
                                                VK_NULL_HANDLE,
                                                &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // Wait on previous fence if this image is already in flight
        if (imagesInFlight.size() > imageIndex && imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }

        static float rotationAngle = 0.0f;
        rotationAngle += deltaTime * glm::radians(45.0f); // rotate 45 degrees per second

        // Update uniform buffers
        for (Entity e : ecsWorld->getAllEntitiesWith<UniformBufferComponent, TransformComponent>()) {
            auto& uboComp = ecsWorld->getComponent<UniformBufferComponent>(e);
            auto& transform = ecsWorld->getComponent<TransformComponent>(e);

            UniformBufferObject ubo{};
            glm::mat4 model = transform.getModelMatrix();

            // Apply additional rotation around Y-axis
            model = glm::rotate(model, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));

            ubo.model = model;
            ubo.view = vcamera->getViewMatrix();
            ubo.proj = vcamera->getProjectionMatrix();

            memcpy(uboComp.ubPtr->uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
        }

        // Reset this frame's fence before submitting
        vkResetFences(device, 1, &currentFence);

        // Reset & record command buffer
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { fso->getImageAvailableSemaphore(currentFrame) };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        // --- use per-swapchain-image semaphore ---
        VkSemaphore signalSemaphores[] = { fso->getRenderFinishedSemaphore(imageIndex) };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(vContext.get()->getLogicalDeviceWrapper()->getGraphicsQueue(),
                        1, &submitInfo, currentFence) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        // Mark this image as in-flight on this fence
        if (imagesInFlight.size() > imageIndex) {
            imagesInFlight[imageIndex] = currentFence;
        }

        // Optional frame-end event
        if (lastFrameTime >= frameInterval) {
            onFrameEndedEvent.notify(deltaTime, imageIndex);
            lastFrameTime -= frameInterval;
            frameCounter++;
        }

        // Present
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { vContext.get()->getSwapChainWrapper()->getSwapChain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(vContext.get()->getLogicalDeviceWrapper()->getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % GraphicsConstants::MAX_FRAMES_IN_FLIGHT;
    }



    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};

#endif // GRAVITAS_HPP