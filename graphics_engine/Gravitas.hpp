#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/hash.hpp>

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

#include "GravitasEngineConstants.h"
#include "Vertex.h"
#include "UniformBufferObject.h"

#include "GTSGLFWOutputWindow.hpp"
#include "GLFWWindowSurface.hpp"
#include "VulkanPhysicalDevice.hpp"
#include "VulkanLogicalDevice.hpp"
#include "VulkanSwapChain.hpp"
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
    GTSOutputWindow* vwindow;
    VulkanInstance* vinstance;
    WindowSurface* vsurface;
    VulkanPhysicalDevice* vphysicaldevice;
    VulkanLogicalDevice* vlogicaldevice;
    VulkanSwapChain* vswapchain;
    VulkanRenderer* vrenderer;
    VulkanRenderPass* vrenderpass;
    GTSDescriptorSetManager* vdescriptorsetmanager;
    VulkanPipeline* vpipeline;
    GTSFramebufferManager* vframebuffer;
    GtsCamera* vcamera;

    GtsScene* currentScene;
    GtsSceneNode* selectedNode;

    GtsOnKeyPressedEvent onKeyPressedEvent;

    void subscribeOnKeyPressedEvent(std::function<void(int key, int scancode, int action, int mods)> f)
    {
        onKeyPressedEvent.subscribe(f);
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

    GtsRenderableObject* createObject(std::string model_path, std::string texture_path)
    {
        GtsRenderableObject* vobject = new GtsRenderableObject(vlogicaldevice, vphysicaldevice, vrenderer, model_path, texture_path, GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        vdescriptorsetmanager->allocateDescriptorSets(vobject);
        return vobject;
    }

    void init(uint32_t width, uint32_t height, std::string title, bool enableValidationLayers)
    {
        vwindow = new GTSGLFWOutputWindow();
        vwindow->init(width, height, title, enableValidationLayers);
        vwindow->setOnWindowResizeCallback(std::bind(&Gravitas::OnFrameBufferResizeCallback, this, std::placeholders::_1, std::placeholders::_2));
        vwindow->setOnKeyPressedCallback(std::bind(&Gravitas::OnKeyPressedCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        vinstance = new VulkanInstance(enableValidationLayers, vwindow);
        vsurface = new GLFWWindowSurface(vwindow, vinstance);
        vphysicaldevice = new VulkanPhysicalDevice(vinstance, vsurface);
        vlogicaldevice = new VulkanLogicalDevice(vinstance, vphysicaldevice, enableValidationLayers);
        vswapchain = new VulkanSwapChain(vwindow, vsurface, vphysicaldevice, vlogicaldevice);
        vrenderer = new VulkanRenderer(vlogicaldevice, vphysicaldevice, vswapchain);
        vrenderpass = new VulkanRenderPass();
        vrenderpass->init(vswapchain, vlogicaldevice, vrenderer);
        vdescriptorsetmanager = new GTSDescriptorSetManager(vlogicaldevice, GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        vpipeline = new VulkanPipeline(vlogicaldevice, vdescriptorsetmanager, vrenderpass, {GravitasEngineConstants::V_SHADER_PATH, GravitasEngineConstants::F_SHADER_PATH});
        vframebuffer = new GTSFramebufferManager(vlogicaldevice, vswapchain, vrenderer, vrenderpass);
        vcamera = new GtsCamera(vswapchain->getSwapChainExtent());
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

        //std::cout << "Current Root Nodes: " <<  currentScene->countRootNodes() << std::endl;
    }

    void run() 
    {
        while (!vwindow->shouldClose())
        {
            vwindow->pollEvents();

            if(!currentScene->empty())
            {
                drawFrame();
            }
        }

        vkDeviceWaitIdle(vlogicaldevice->getDevice());

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
        delete currentScene;
        delete vcamera;
        delete vframebuffer;
        delete vpipeline;
        delete vdescriptorsetmanager;
        delete vrenderpass;
        delete vrenderer;
        for (size_t i = 0; i < GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT; i++) 
        {
            vkDestroySemaphore(vlogicaldevice->getDevice(), renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(vlogicaldevice->getDevice(), imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(vlogicaldevice->getDevice(), inFlightFences[i], nullptr);
        }
        delete vswapchain;
        delete vlogicaldevice;
        delete vphysicaldevice;
        delete vsurface;
        delete vinstance;
        delete vwindow;
    }

    //we can do that once we figured out how to put everything into classes
    void recreateSwapChain() 
    {
        // int width = 0, height = 0;
        // vwindow->getSize(width, height);

        // while (width == 0 || height == 0) 
        // {
        //     vwindow->getSize(width, height);
        //     vwindow->pollEvents();
        // }

        // std::cout << "swapchain recreated" << std::endl;
        // std::cout << width << std::endl;
        // std::cout << height << std::endl;

        // vkDeviceWaitIdle(vlogicaldevice->getDevice());

        //cleanupSwapChain();

        //delete vswapchain;
        //vswapchain = new VulkanSwapChain(vwindow, vsurface, vphysicaldevice, vlogicaldevice);
        //createDepthResources();
        //createFramebuffers();
    }

    // bool hasStencilComponent(VkFormat format) {
    //     return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    // }

    void createCommandBuffers() 
    {
        commandBuffers.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = vlogicaldevice->getCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(vlogicaldevice->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
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
        renderPassInfo.renderArea.extent = vswapchain->getSwapChainExtent();

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
            viewport.width = (float) vswapchain->getSwapChainExtent().width;
            viewport.height = (float) vswapchain->getSwapChainExtent().height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = vswapchain->getSwapChainExtent();
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            
            //call draw for all objects here :)
            currentScene->render(commandBuffer, vpipeline->getPipelineLayout(), currentFrame);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(vlogicaldevice->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vlogicaldevice->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vlogicaldevice->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    void drawFrame() 
    {
        //correct delta time calculations this time around
        static auto previousTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();
        previousTime = currentTime;

        vkWaitForFences(vlogicaldevice->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vlogicaldevice->getDevice(), vswapchain->getSwapChain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        currentScene->update(*vcamera, GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT, deltaTime);

        vkResetFences(vlogicaldevice->getDevice(), 1, &inFlightFences[currentFrame]);

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

        if (vkQueueSubmit(vlogicaldevice->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {vswapchain->getSwapChain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(vlogicaldevice->getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            std::cout << "FrameBuffer was resized! recreating swapchain" << std::endl;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};