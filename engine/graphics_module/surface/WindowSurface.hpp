#pragma once

#include <vulkan/vulkan.h>

#include "WindowSurfaceConfig.h"

class WindowSurface 
{
    protected:
        WindowSurfaceConfig config;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        explicit WindowSurface(const WindowSurfaceConfig& config): config(config) {};
        virtual void init() = 0;

    public:
        virtual ~WindowSurface() = default;
        virtual VkSurfaceKHR& getSurface() = 0;
};