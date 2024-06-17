#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

#include "VulkanPhysicalDevice.hpp"
#include "VulkanSwapChain.hpp"

class VulkanRenderer 
{
    public:
        VulkanRenderer(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice, VulkanSwapChain* vswapchain);
        ~VulkanRenderer();

        VkImage& getDepthImage();
        VkDeviceMemory& getDepthImageMemory();
        VkImageView& getDepthImageView();

        void createDepthResources();
        VkFormat findDepthFormat();
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

    private:
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VulkanLogicalDevice* vlogicaldevice;
        VulkanPhysicalDevice* vphysicaldevice;
        VulkanSwapChain* vswapchain;

        void cleanup();
};

#endif // VULKAN_RENDERER_HPP
