#pragma once

#include <vector>
#include <string>
#include <vulkan/vulkan.h>

// config for a concrete Vulkan Renderer
struct RendererConfig 
{
    bool headless = false;
    uint32_t renderWidth = 1;
    uint32_t renderHeight = 1;
    uint32_t maxScreenshotsPerRun = 64;
    float minSecondsBetweenScreenshots = 0.25f;
};
