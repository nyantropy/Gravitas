#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>

#include "VulkanRenderPass.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanLogicalDevice.hpp"

#include "Attachment.hpp"


class GTSFramebufferManager
{
    public:
        GTSFramebufferManager(VulkanLogicalDevice* vlogicaldevice, VulkanSwapChain* vswapchain, VulkanRenderPass* vrenderpass, Attachment* att);
        ~GTSFramebufferManager();

        std::vector<VkFramebuffer>& getFramebuffers();

    private:
        VulkanRenderPass* vrenderpass;
        VulkanSwapChain* vswapchain;
        VulkanLogicalDevice* vlogicaldevice;
        Attachment* att;

        std::vector<VkFramebuffer> swapChainFramebuffers;

        void createFramebuffers();
};