#include "GLFWOutputWindow.hpp"

GLFWOutputWindow::GLFWOutputWindow(OutputWindowConfig config): OutputWindow(config)
{
    this->init();
}

GLFWOutputWindow::~GLFWOutputWindow()
{
    if(static_cast<GLFWwindow*>(this->window))
    {
        glfwDestroyWindow(static_cast<GLFWwindow*>(this->window));
        glfwTerminate();
    }
}

void GLFWOutputWindow::init() 
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    this->window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(static_cast<GLFWwindow*>(this->window), this);
    glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(this->window), framebufferResizeCallbackStatic);
    glfwSetKeyCallback(static_cast<GLFWwindow*>(this->window), onKeyPressedCallbackStatic);
}

bool GLFWOutputWindow::shouldClose() const 
{
    return glfwWindowShouldClose(static_cast<GLFWwindow*>(this->window));
}

void GLFWOutputWindow::pollEvents() 
{
    glfwPollEvents();
}

void GLFWOutputWindow::getSize(int& width, int& height) const 
{
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(this->window), &width, &height);
}

void* GLFWOutputWindow::getWindow() const 
{
    return static_cast<GLFWwindow*>(this->window);
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

// return a unique pointer with the glfw surface handle wrapper
std::unique_ptr<VulkanSurface> GLFWOutputWindow::createSurface(VulkanSurfaceConfig config) const
{
    config.nativeWindow = this->window;
    return std::make_unique<GLFWVulkanSurface>(config);
}