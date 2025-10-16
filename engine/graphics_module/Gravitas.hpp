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

#include "VulkanRenderer.hpp"
#include "VulkanRenderPass.hpp"
#include "GTSDescriptorSetManager.hpp"
#include "VulkanShader.hpp"
#include "VulkanPipeline.hpp"
#include "GTSFramebufferManager.hpp"
#include "GtsBufferService.hpp"
#include "VulkanTexture.hpp"
#include "GtsModelLoader.hpp"
#include "GtsCamera.hpp"
#include "GtsRenderableObject.hpp"
#include "GtsSceneNode.hpp"
#include "GtsScene.hpp"
#include "GtsSceneNodeOpt.h"
#include "GtsOnKeyPressedEvent.hpp"
#include "GtsOnSceneUpdatedEvent.hpp"
#include "GtsOnFrameEndedEvent.hpp"
#include "GtsEncoder.hpp"
#include "GtsFrameGrabber.hpp"

// Vulkan Context include
#include "VulkanContext.hpp"
#include "VulkanContextConfig.h"

// output window includes
#include "OutputWindowConfig.h"
#include "GLFWOutputWindow.hpp"

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

    std::unique_ptr<VulkanContext> vContext;
    std::unique_ptr<OutputWindow> outputWindow;

    VulkanRenderer* vrenderer;
    VulkanRenderPass* vrenderpass;
    GTSDescriptorSetManager* vdescriptorsetmanager;
    VulkanPipeline* vpipeline;
    GTSFramebufferManager* vframebuffer;
    GtsCamera* vcamera;

    GtsScene* currentScene;
    GtsSceneNode* selectedNode;

    GtsOnKeyPressedEvent onKeyPressedEvent;
    GtsOnSceneUpdatedEvent onSceneUpdatedEvent;
    GtsOnFrameEndedEvent onFrameEndedEvent;

    GtsFrameGrabber* framegrabber;
    GtsEncoder* encoder;

    void subscribeOnKeyPressedEvent(std::function<void(int key, int scancode, int action, int mods)> f)
    {
        onKeyPressedEvent.subscribe(f);
    }

    void subscribeOnSceneUpdatedEvent(std::function<void()> f)
    {
        onSceneUpdatedEvent.subscribe(f);
    }

    void subscribeOnFrameEndedEvent(std::function<void(int, uint32_t)> f)
    {
        onFrameEndedEvent.subscribe(f);
    }

    //select a node in the engine based on its identifier
    bool selectNode(std::string identifier)
    {
        selectedNode = currentScene->search(identifier);

        if(selectedNode == nullptr)
        {
            return false;
        }

        return true;
    }

    GtsSceneNode* getSelectedSceneNodePtr()
    {
        return selectedNode;
    }

    GtsSceneNode* getSceneNodePtr(std::string identifier)
    {
        return currentScene->search(identifier);
    }

    void splitSceneNode(GtsSceneNode* node)
    {
        currentScene->splitUpNode(node);
    }

    GtsRenderableObject* createObject(std::string model_path, std::string texture_path)
    {
        GtsRenderableObject* vobject = new GtsRenderableObject(vContext.get()->getLogicalDeviceWrapper(), vContext.get()->getPhysicalDeviceWrapper(), vdescriptorsetmanager, vrenderer, model_path, texture_path, GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
        return vobject;
    }

    void init(uint32_t width, uint32_t height, std::string title)
    {
        // output window settings
        OutputWindowConfig owConfig;
        owConfig.enableValidationLayers = true;
        owConfig.width = width;
        owConfig.height = height;
        owConfig.title = title;

        // setup an output window
        outputWindow = std::make_unique<GLFWOutputWindow>(owConfig);
        outputWindow->setOnWindowResizeCallback(
        std::bind(&Gravitas::OnFrameBufferResizeCallback, this, std::placeholders::_1, std::placeholders::_2));
        outputWindow->setOnKeyPressedCallback(
        std::bind(&Gravitas::OnKeyPressedCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

        VulkanContextConfig vcConfig;
        vcConfig.enableValidationLayers = enableValidationLayers;
        vcConfig.vulkanInstanceExtensions = outputWindow->getRequiredExtensions();
        vcConfig.outputWindowPtr = outputWindow.get();

        vContext = std::make_unique<VulkanContext>(vcConfig);


        vrenderer = new VulkanRenderer(vContext.get()->getLogicalDeviceWrapper(), vContext.get()->getPhysicalDeviceWrapper(), vContext.get()->getSwapChainWrapper());
        vrenderpass = new VulkanRenderPass();
        vrenderpass->init(vContext.get()->getSwapChainWrapper(), vContext.get()->getLogicalDeviceWrapper(), vrenderer);
        vdescriptorsetmanager = new GTSDescriptorSetManager(vContext.get()->getLogicalDeviceWrapper(), GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
        vpipeline = new VulkanPipeline(vContext.get()->getLogicalDeviceWrapper(), vdescriptorsetmanager, vrenderpass, {GraphicsConstants::V_SHADER_PATH, GraphicsConstants::F_SHADER_PATH});
        vframebuffer = new GTSFramebufferManager(vContext.get()->getLogicalDeviceWrapper(), vContext.get()->getSwapChainWrapper(), vrenderer, vrenderpass);
        vcamera = new GtsCamera(vContext.get()->getSwapChainWrapper()->getSwapChainExtent());
        createCommandBuffers();
        createSyncObjects();
    }

    void createEmptyScene()
    {
        currentScene = new GtsScene();
    }

    void addNodeToScene(GtsSceneNodeOpt options)
    {
        GtsSceneNode* node = new GtsSceneNode(options.objectPtr, options.animPtr, options.identifier);

        if(options.needsActivation)
        {
            node->disableRendering();
            node->disableAnimation();
            node->disableUpdating();
        }

        if(options.translationVector != glm::vec3(0.0f, 0.0f, 0.0f))
        {
            node->translate(options.translationVector);
        }

        if(options.rotationVector != glm::vec3(0.0f, 0.0f, 0.0f))
        {
            node->rotate(options.rotationVector);
        }

        if(options.scaleVector != glm::vec3(0.0f, 0.0f, 0.0f))
        {
            node->scale(options.scaleVector);
        }

        if(options.parentIdentifier.empty())
        {
            currentScene->addNode(node);
        }
        else
        {
            currentScene->addNodeToParent(node, options.parentIdentifier);
        }
    }

    void removeNodeFromScene(GtsSceneNode* node)
    {
        currentScene->removeNode(node);
    }

    void startEncoder()
    {
        framegrabber = new GtsFrameGrabber(vContext.get()->getSwapChainWrapper(), vrenderer, vContext.get()->getLogicalDeviceWrapper(), vContext.get()->getPhysicalDeviceWrapper());
        encoder = new GtsEncoder(framegrabber);
        onFrameEndedEvent.subscribe(std::bind(&GtsEncoder::onFrameEnded, encoder, std::placeholders::_1, std::placeholders::_2));
    }

    void run() 
    {
        while (!outputWindow->shouldClose())
        {
            outputWindow->pollEvents();

            if(!currentScene->empty())
            {
                drawFrame();
            }
        }

        vkDeviceWaitIdle(vContext.get()->getLogicalDeviceWrapper()->getDevice());

        cleanup();
    }

private:
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    void OnFrameBufferResizeCallback(int width, int height) 
    {
        framebufferResized = true;
    }

    void OnKeyPressedCallback(int key, int scancode, int action, int mods)
    {
        onKeyPressedEvent.notify(key, scancode, action, mods);
    }

    void cleanup() 
    {
        if(encoder != nullptr)
        {
            delete encoder;
        }

        delete currentScene;
        delete vcamera;
        delete vframebuffer;
        delete vpipeline;
        delete vdescriptorsetmanager;
        delete vrenderpass;
        delete vrenderer;
        for (size_t i = 0; i < GraphicsConstants::MAX_FRAMES_IN_FLIGHT; i++) 
        {
            vkDestroySemaphore(vContext.get()->getLogicalDeviceWrapper()->getDevice(), renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(vContext.get()->getLogicalDeviceWrapper()->getDevice(), imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(vContext.get()->getLogicalDeviceWrapper()->getDevice(), inFlightFences[i], nullptr);
        }
        vContext.reset();
        outputWindow.reset();
    }

    //we can do that once we figured out how to put everything into classes
    void recreateSwapChain() 
    {
        // int width = 0, height = 0;
        // outputWindow->getSize(width, height);

        // while (width == 0 || height == 0) 
        // {
        //     outputWindow->getSize(width, height);
        //     outputWindow->pollEvents();
        // }

        // std::cout << "swapchain recreated" << std::endl;
        // std::cout << width << std::endl;
        // std::cout << height << std::endl;

        // vkDeviceWaitIdle(vContext.get()->getLogicalDeviceWrapper()->getDevice());

        //cleanupSwapChain();

        //delete vContext.get()->getSwapChainWrapper();
        //vContext.get()->getSwapChainWrapper() = new VulkanSwapChain(outputWindow, vContext.get()->getSurfaceWrapper(), vContext.get()->getPhysicalDeviceWrapper(), vContext.get()->getLogicalDeviceWrapper());
        //createDepthResources();
        //createFramebuffers();
    }

    // bool hasStencilComponent(VkFormat format) {
    //     return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    // }

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
        renderPassInfo.renderPass = vrenderpass->getRenderPass();
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

            
            //call draw for all objects here :)
            currentScene->render(commandBuffer, vpipeline->getPipelineLayout(), currentFrame);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < GraphicsConstants::MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(vContext.get()->getLogicalDeviceWrapper()->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vContext.get()->getLogicalDeviceWrapper()->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vContext.get()->getLogicalDeviceWrapper()->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    float FPS = 30.0f;
    double frameInterval = 1.0 / FPS;
    double lastFrameTime = 0.0;
    int frameCounter = 0;

    void drawFrame() 
    {
        //correct delta time calculations this time around
        static auto previousTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();
        previousTime = currentTime;
        lastFrameTime += deltaTime;

        vkWaitForFences(vContext.get()->getLogicalDeviceWrapper()->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vContext.get()->getLogicalDeviceWrapper()->getDevice(), vContext.get()->getSwapChainWrapper()->getSwapChain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) 
        {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) 
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        std::cout << "before scene update" << std::endl;

        currentScene->update(*vcamera, GraphicsConstants::MAX_FRAMES_IN_FLIGHT, deltaTime);
        onSceneUpdatedEvent.notify();

        vkResetFences(vContext.get()->getLogicalDeviceWrapper()->getDevice(), 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(vContext.get()->getLogicalDeviceWrapper()->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        std::cout << "before frame ended check" << std::endl;
        if (lastFrameTime >= frameInterval) 
        {
            onFrameEndedEvent.notify(deltaTime, imageIndex);
            lastFrameTime -= frameInterval;
            frameCounter++;
        }

        // uint8_t* bufferData = new uint8_t[vContext.get()->getSwapChainWrapper()->getSwapChainExtent().width * vContext.get()->getSwapChainWrapper()->getSwapChainExtent().height * 4];
        // getCurrentFrame(imageIndex, bufferData);
        // encodeAndWriteFrame(bufferData, vContext.get()->getSwapChainWrapper()->getSwapChainExtent().width, vContext.get()->getSwapChainWrapper()->getSwapChainExtent().height);
        // delete bufferData;

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {vContext.get()->getSwapChainWrapper()->getSwapChain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(vContext.get()->getLogicalDeviceWrapper()->getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            std::cout << "FrameBuffer was resized! recreating swapchain" << std::endl;
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