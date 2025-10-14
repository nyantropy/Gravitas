#pragma once

#include "WindowSurface.hpp"
#include "OutputWindow.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>

class GLFWWindowSurface : public WindowSurface 
{
    public:
        GLFWWindowSurface(OutputWindow* vwindow, VulkanInstance* vinstance);
        ~GLFWWindowSurface();
        VkSurfaceKHR& getSurface() override;

    private:
        GLFWwindow* window;
};
