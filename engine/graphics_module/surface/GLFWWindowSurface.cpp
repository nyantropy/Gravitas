#include "GLFWWindowSurface.hpp"

GLFWWindowSurface::GLFWWindowSurface(WindowSurfaceConfig config): WindowSurface(config)
{
    this->init();
    //this->window = static_cast<GLFWwindow*>(vwindow->getWindow());
}

GLFWWindowSurface::~GLFWWindowSurface()
{
    vkDestroySurfaceKHR(this->config.instance, surface, nullptr);
}

void GLFWWindowSurface::init()
{
    if(surface == nullptr)
    {
        if (glfwCreateWindowSurface(this->config.instance, static_cast<GLFWwindow*>(this->config.nativeWindow), nullptr, &surface) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }
}

VkSurfaceKHR& GLFWWindowSurface::getSurface() 
{
    return surface;
}
