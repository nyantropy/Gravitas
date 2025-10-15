#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <stdexcept>

#include "WindowSurface.hpp"
#include "WindowSurfaceConfig.h"

class GLFWWindowSurface : public WindowSurface 
{
    public:
        GLFWWindowSurface(WindowSurfaceConfig config);
        ~GLFWWindowSurface();
        VkSurfaceKHR& getSurface() override;

    private:
        void init() override;
};
