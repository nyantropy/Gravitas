#pragma once

#include <vulkan/vulkan.h>

struct AttachmentConfig 
{
    uint32_t width = 1;
    uint32_t height = 1;

    // image 
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageTiling tiling;
    VkImageUsageFlags imageUsageFlags;

    // memory
    VkMemoryPropertyFlags memoryPropertyFlags;

    // image view
    VkImageAspectFlags imageAspectFlags;
};
