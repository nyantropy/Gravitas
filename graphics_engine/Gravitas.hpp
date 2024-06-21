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

const std::string MODEL_PATH = "resources/viking_room.obj";
const std::string TEXTURE_PATH = "resources/viking_room.png";

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
    VulkanTexture* vtexture;
    GtsCamera* vcamera;
    

    void run() 
    {
        vwindow = new GTSGLFWOutputWindow();
        vwindow->init(GravitasEngineConstants::GLFW_DEFAULT_WIDTH, GravitasEngineConstants::GLFW_DEFAULT_HEIGHT, "Title", enableValidationLayers);
        vwindow->setOnWindowResizeCallback(std::bind(&Gravitas::OnFrameBufferResizeCallback, this, std::placeholders::_1, std::placeholders::_2));

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
        vtexture = new VulkanTexture(vlogicaldevice, vphysicaldevice, vrenderer, TEXTURE_PATH);
        vcamera = new GtsCamera(vswapchain->getSwapChainExtent());


        GtsRenderableObject object = GtsRenderableObject(vlogicaldevice, vphysicaldevice, vrenderer, MODEL_PATH, TEXTURE_PATH, GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        

        GtsModelLoader::loadModel(MODEL_PATH, vertices, indices);
        GtsBufferService::createVertexBuffer(vlogicaldevice, vphysicaldevice, vertices, vertexBuffer, vertexBufferMemory);
        GtsBufferService::createIndexBuffer(vlogicaldevice, vphysicaldevice, indices, indexBuffer, indexBufferMemory);
        GtsBufferService::createUniformBuffers(vlogicaldevice, vphysicaldevice,
         uniformBuffers, uniformBuffersMemory, uniformBuffersMapped, GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
        mainLoop();
        cleanup();

        delete vcamera;
        delete vtexture;
        delete vframebuffer;
        delete vpipeline;
        delete vdescriptorsetmanager;
        delete vrenderpass;
        delete vrenderer;
        delete vswapchain;
        delete vlogicaldevice;
        delete vphysicaldevice;
        delete vsurface;
        delete vinstance;
        delete vwindow;
    }

private:

    //VkDebugUtilsMessengerEXT debugMessenger;
    
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    std::vector<VkDescriptorSet> descriptorSets;

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

    void mainLoop() 
    {
        while (!vwindow->shouldClose())
        {
            vwindow->pollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(vlogicaldevice->getDevice());
    }

    void cleanup() 
    {
        for (size_t i = 0; i < GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(vlogicaldevice->getDevice(), uniformBuffers[i], nullptr);
            vkFreeMemory(vlogicaldevice->getDevice(), uniformBuffersMemory[i], nullptr);
        }

        vkDestroyBuffer(vlogicaldevice->getDevice(), indexBuffer, nullptr);
        vkFreeMemory(vlogicaldevice->getDevice(), indexBufferMemory, nullptr);

        vkDestroyBuffer(vlogicaldevice->getDevice(), vertexBuffer, nullptr);
        vkFreeMemory(vlogicaldevice->getDevice(), vertexBufferMemory, nullptr);

        for (size_t i = 0; i < GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(vlogicaldevice->getDevice(), renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(vlogicaldevice->getDevice(), imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(vlogicaldevice->getDevice(), inFlightFences[i], nullptr);
        }
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

    //setup descriptor sets based on frames in flight
    void createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT, vdescriptorsetmanager->getDescriptorSetLayout());
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = vdescriptorsetmanager->getDescriptorPool();
        allocInfo.descriptorSetCount = static_cast<uint32_t>(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(vlogicaldevice->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = vtexture->getTextureImageView();
            imageInfo.sampler = vtexture->getTextureSampler();

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(vlogicaldevice->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void createCommandBuffers() {
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

            VkBuffer vertexBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vpipeline->getPipelineLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

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

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = vcamera->getViewMatrix();
        ubo.proj = vcamera->getProjectionMatrix();

        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void drawFrame() {
        vkWaitForFences(vlogicaldevice->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vlogicaldevice->getDevice(), vswapchain->getSwapChain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer(currentFrame);

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