#include "GLFWVulkanSurface.hpp"

GLFWVulkanSurface::GLFWVulkanSurface(VulkanSurfaceConfig config): VulkanSurface(config)
{
    this->init();
    //this->window = static_cast<GLFWwindow*>(vwindow->getWindow());
}

GLFWVulkanSurface::~GLFWVulkanSurface()
{
    vkDestroySurfaceKHR(this->config.instance, surface, nullptr);
}

void GLFWVulkanSurface::init()
{
    if(surface == VK_NULL_HANDLE)
    {
        if (glfwCreateWindowSurface(this->config.instance, static_cast<GLFWwindow*>(this->config.nativeWindow), nullptr, &surface) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }
}

VkSurfaceKHR& GLFWVulkanSurface::getSurface() 
{
    return surface;
}
