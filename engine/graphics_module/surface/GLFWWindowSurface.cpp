#include "GLFWWindowSurface.hpp"

GLFWWindowSurface::GLFWWindowSurface(OutputWindow* vwindow, VulkanInstance* vinstance)
{
    this->window = static_cast<GLFWwindow*>(vwindow->getWindow());
    this->vinstance = vinstance;
}

GLFWWindowSurface::~GLFWWindowSurface()
{
    vkDestroySurfaceKHR(vinstance->getInstance(), surface, nullptr);
}

VkSurfaceKHR& GLFWWindowSurface::getSurface() 
{
    if(surface == nullptr)
    {
        if (glfwCreateWindowSurface(vinstance->getInstance(), window, nullptr, &surface) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    return surface;
}
