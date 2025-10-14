#include "GLFWOutputWindow.hpp"

GLFWOutputWindow::GLFWOutputWindow(OutputWindowConfig config): OutputWindow(config)
{
    this->window = nullptr;
    this->init();
}

GLFWOutputWindow::~GLFWOutputWindow()
{
    if(window)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

void GLFWOutputWindow::init() 
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallbackStatic);
    glfwSetKeyCallback(window, onKeyPressedCallbackStatic);
}

void GLFWOutputWindow::setOnWindowResizeCallback(const std::function<void(int, int)>& callback) 
{
    resizeCallback = callback;
}

void GLFWOutputWindow::setOnKeyPressedCallback(const std::function<void(int, int, int, int)>& callback)
{
    onKeyPressedCallback = callback;
}

bool GLFWOutputWindow::shouldClose() const 
{
    return glfwWindowShouldClose(window);
}

void GLFWOutputWindow::pollEvents() 
{
    glfwPollEvents();
}

void GLFWOutputWindow::getSize(int& width, int& height) const 
{
    glfwGetFramebufferSize(window, &width, &height);
}

void* GLFWOutputWindow::getWindow() const 
{
    return window;
}

std::vector<const char*> GLFWOutputWindow::getRequiredExtensions() const
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (config.enableValidationLayers) 
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}