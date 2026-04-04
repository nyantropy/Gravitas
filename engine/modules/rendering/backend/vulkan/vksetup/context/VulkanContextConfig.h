#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <string>

#include "OutputWindow.hpp"
#include "../../../../core/init/GraphicsConfig.h"

// all configuration settings for the Vulkan Context object
struct VulkanContextConfig 
{
    // needed for validation
    bool enableValidationLayers;

    // needed for VulkanInstance
    std::vector<const char*> vulkanInstanceExtensions;
    std::string applicationName;

    // needed for WindowSurface
    OutputWindow* outputWindowPtr;
    PresentModePreference presentModePreference = PresentModePreference::Immediate;


    bool enableSurfaceSupport;

    //std::string appname;

    //std::vector<const char*> extensions;
};
