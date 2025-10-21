#pragma once

#include <vulkan/vulkan.h>

struct FrameSyncObjectsConfig
{
    VkDevice vkDevice;
    uint32_t maxFramesInFlight = 2;
};