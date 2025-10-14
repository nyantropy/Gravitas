#pragma once

#include <vector>
#include <vulkan/vulkan.h>

// configuration settings for the VulkanInstance wrapper object
struct VulkanInstanceConfig 
{
    bool enableValidationLayers;
    bool enableSurfaceSupport;

    std::string appname;

    std::vector<const char*> extensions;
};