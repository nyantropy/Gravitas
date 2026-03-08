#pragma once

// Intentional: VK_MAKE_API_VERSION is required to encode the Vulkan version field.
// This struct is Vulkan-specific configuration used only by the graphics backend.
#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

struct GraphicsOptions
{
    char* appName = "TestApp";
    uint32_t appVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
};
