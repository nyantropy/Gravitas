#pragma once

#include <vulkan/vulkan.h>

#include "VulkanSwapChain.hpp"
#include "VulkanLogicalDevice.hpp"
#include "VulkanRenderer.hpp"

class VulkanRenderPass
{
    private:
        VkRenderPass renderPass;
        
        VulkanSwapChain* vswapchain;
        VulkanLogicalDevice* vlogicaldevice;
        VulkanRenderer* vrenderer;

        void createRenderPass();
    public:
        VulkanRenderPass();
        ~VulkanRenderPass();
        void init(VulkanSwapChain* vswapchain, VulkanLogicalDevice* vlogicaldevice, VulkanRenderer* vrenderer);
        VkRenderPass& getRenderPass();
};