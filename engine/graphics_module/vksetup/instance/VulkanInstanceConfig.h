#pragma once

#include <vector>
#include <string>
#include <vulkan/vulkan.h>

// configuration settings for the VulkanInstance wrapper object
struct VulkanInstanceConfig 
{
    bool enableValidationLayers;
    bool enableSurfaceSupport;

    std::string applicationName;

    std::vector<const char*> extensions;

    std::string engineName = "Gravitas";
    uint32_t engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
};