#pragma once

#include <vulkan/vulkan.h>
#include <array>

#include "VulkanLogicalDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanRenderer.hpp"

class VulkanSwapChain;
class VulkanRenderer;

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
