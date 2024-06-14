#pragma once

#include "WindowSurface.hpp"
#include "GTSOutputWindow.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>

class GLFWWindowSurface : public WindowSurface 
{
    public:
        GLFWWindowSurface(GTSOutputWindow* vwindow, VulkanInstance* vinstance);
        ~GLFWWindowSurface();
        VkSurfaceKHR& getSurface() override;

    private:
        GLFWwindow* window;
};
