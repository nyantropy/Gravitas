#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>

#include "VulkanRenderer.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanLogicalDevice.hpp"

class VulkanSwapChain;
class VulkanRenderer;

class GTSFramebufferManager
{
    public:
        GTSFramebufferManager(VulkanLogicalDevice* vlogicaldevice, VulkanSwapChain* vswapchain, VulkanRenderer* vrenderer, VulkanRenderPass* vrenderpass);
        ~GTSFramebufferManager();

        std::vector<VkFramebuffer>& getFramebuffers();

    private:
        VulkanRenderPass* vrenderpass;
        VulkanRenderer* vrenderer;
        VulkanSwapChain* vswapchain;
        VulkanLogicalDevice* vlogicaldevice;

        std::vector<VkFramebuffer> swapChainFramebuffers;

        void createFramebuffers();
};