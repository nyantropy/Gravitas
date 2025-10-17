#pragma once

#include <vector>
#include <stdexcept>
#include <vulkan/vulkan.h>

class FormatUtil
{
    public:
        static VkFormat findSupportedFormat(VkPhysicalDevice& physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) 
        {
            for (VkFormat format : candidates) 
            {
                VkFormatProperties props;
                vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

                if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) 
                {
                    return format;
                }
                else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) 
                {
                    return format;
                }
            }

            throw std::runtime_error("failed to find supported format!");
        }
};