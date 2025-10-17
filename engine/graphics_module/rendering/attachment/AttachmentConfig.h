#pragma once

#include <vulkan/vulkan.h>

struct AttachmentConfig 
{
    // core vulkan structures
    VkDevice vkDevice = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
    VkExtent2D vkExtent;

    // image 
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageTiling tiling;
    VkImageUsageFlags imageUsageFlags;

    // memory
    VkMemoryPropertyFlags memoryPropertyFlags;

    // image view
    VkImageAspectFlags imageAspectFlags;
};