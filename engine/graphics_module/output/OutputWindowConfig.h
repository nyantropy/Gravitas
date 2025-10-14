#pragma once

#include <vector>
#include <string>
#include <vulkan/vulkan.h>

// configuration settings for the OutputWindow wrapper object
struct OutputWindowConfig 
{
    int width;
    int height;
    std::string title;
    
    bool enableValidationLayers;
};