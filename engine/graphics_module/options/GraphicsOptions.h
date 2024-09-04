#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

struct GraphicsOptions
{
    char* appName = "TestApp";
    uint32_t appVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
};
