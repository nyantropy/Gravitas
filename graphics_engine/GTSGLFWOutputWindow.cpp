#include "GTSGLFWOutputWindow.hpp"

GTSGLFWOutputWindow::GTSGLFWOutputWindow()
{
    this->window = nullptr;
}

GTSGLFWOutputWindow::~GTSGLFWOutputWindow()
{
    if(window)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

void GTSGLFWOutputWindow::init(int width, int height, const std::string& title, bool enableValidationLayers) 
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallbackStatic);

    this->enableValidationLayers = enableValidationLayers;
}

void GTSGLFWOutputWindow::setOnWindowResizeCallback(const std::function<void(int, int)>& callback) 
{
    resizeCallback = callback;
}

bool GTSGLFWOutputWindow::shouldClose() const 
{
    return glfwWindowShouldClose(window);
}

void GTSGLFWOutputWindow::pollEvents() 
{
    glfwPollEvents();
}

void GTSGLFWOutputWindow::getSize(int& width, int& height) const 
{
    glfwGetFramebufferSize(window, &width, &height);
}

void* GTSGLFWOutputWindow::getWindow() const 
{
    return window;
}

std::vector<const char*> GTSGLFWOutputWindow::getRequiredExtensions() const
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) 
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}