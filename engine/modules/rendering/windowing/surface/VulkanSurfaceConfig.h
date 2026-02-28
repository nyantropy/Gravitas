#pragma once

#include <vulkan/vulkan.h>

// configuration settings for a dedicated window surface object
struct VulkanSurfaceConfig 
{
    VkInstance vkInstance = VK_NULL_HANDLE;
    void* nativeWindow = nullptr; // platform-specific pointer (e.g., GLFWwindow*)
};