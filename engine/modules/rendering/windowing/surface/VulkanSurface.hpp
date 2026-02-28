#pragma once

#include <vulkan/vulkan.h>

#include "VulkanSurfaceConfig.h"

class VulkanSurface 
{
    protected:
        VulkanSurfaceConfig config;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        explicit VulkanSurface(const VulkanSurfaceConfig& config): config(config) {};
        virtual void init() = 0;

    public:
        virtual ~VulkanSurface() = default;
        virtual VkSurfaceKHR& getSurface() = 0;
};