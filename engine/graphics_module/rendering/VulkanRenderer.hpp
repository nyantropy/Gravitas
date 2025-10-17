#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

#include "VulkanPhysicalDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "GtsBufferService.hpp"
#include "MemoryUtil.hpp"

class VulkanRenderer 
{
    public:
        VulkanRenderer(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice, VulkanSwapChain* vswapchain);
        ~VulkanRenderer();

        VkImage& getDepthImage();
        VkDeviceMemory& getDepthImageMemory();
        VkImageView& getDepthImageView();

        VkFormat findDepthFormat();
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
        
        static void transitionImageLayout(VulkanLogicalDevice* vlogicaldevice, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    private:
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VulkanLogicalDevice* vlogicaldevice;
        VulkanPhysicalDevice* vphysicaldevice;
        VulkanSwapChain* vswapchain;

        void createDepthResources();
        void cleanup();
};