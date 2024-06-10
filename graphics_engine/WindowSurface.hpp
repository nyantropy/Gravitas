#pragma once

#include <vulkan/vulkan.h>
#include "VulkanInstance.hpp"

class WindowSurface 
{
    protected:
        VkSurfaceKHR surface = nullptr;
        VulkanInstance* vinstance = nullptr;

    public:
        virtual ~WindowSurface() = default;
        virtual VkSurfaceKHR& getSurface() = 0;
};