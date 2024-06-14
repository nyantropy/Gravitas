#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

#include "VulkanSwapChain.hpp"

class VulkanRenderer 
{
    public:
        VulkanRenderer();
        ~VulkanRenderer();

        void init(VulkanPhysicalDevice* vphysicaldevice);

        void createDepthResources(VkExtent2D extent);
        VkFormat findDepthFormat();
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    private:
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VulkanPhysicalDevice* vphysicaldevice;
};

#endif // VULKAN_RENDERER_HPP
