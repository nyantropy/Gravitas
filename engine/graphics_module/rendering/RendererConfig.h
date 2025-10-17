#pragma once

#include <vector>
#include <string>
#include <vulkan/vulkan.h>

// config for a concrete Vulkan Renderer
struct RendererConfig 
{
    VkPhysicalDevice vkPhysicalDevice;
    VkDevice vkDevice;
    VkExtent2D vkExtent;
};