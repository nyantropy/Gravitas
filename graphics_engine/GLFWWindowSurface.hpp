#pragma once

#include "WindowSurface.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>

class GLFWWindowSurface : public WindowSurface 
{
    public:
        GLFWWindowSurface(GLFWwindow* window, VulkanInstance* vinstance);
        ~GLFWWindowSurface();
        VkSurfaceKHR& getSurface() override;

    private:
        GLFWwindow* window;
};
