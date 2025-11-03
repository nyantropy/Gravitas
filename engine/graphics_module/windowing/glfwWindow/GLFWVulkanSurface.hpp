#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <stdexcept>

#include "VulkanSurface.hpp"
#include "VulkanSurfaceConfig.h"

class GLFWVulkanSurface : public VulkanSurface 
{
    public:
        GLFWVulkanSurface(VulkanSurfaceConfig config);
        ~GLFWVulkanSurface();
        VkSurfaceKHR& getSurface() override;

    private:
        void init() override;
};
