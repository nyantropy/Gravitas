#include "GLFWOutputWindow.hpp"

GLFWOutputWindow::GLFWOutputWindow(OutputWindowConfig config): OutputWindow(config)
{
    this->init();
    this->keyTranslator = std::make_unique<GLFWKeyTranslator>();
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

    if (config.borderlessFullscreen)
    {
        GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode    = glfwGetVideoMode(monitor);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        this->window = glfwCreateWindow(mode->width, mode->height, config.title.c_str(), nullptr, nullptr);
        glfwSetWindowPos(static_cast<GLFWwindow*>(this->window), 0, 0);
    }
    else
    {
        this->window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
    }

    glfwSetWindowUserPointer(static_cast<GLFWwindow*>(this->window), this);
    glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(this->window), framebufferResizeCallbackStatic);
    glfwSetKeyCallback(static_cast<GLFWwindow*>(this->window), onKeyPressedCallbackStatic);
}

void GLFWOutputWindow::setFullscreen()
{
    GLFWwindow* gw = static_cast<GLFWwindow*>(this->window);
    glfwGetWindowPos(gw, &savedX, &savedY);
    glfwGetWindowSize(gw, &savedW, &savedH);

    GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode    = glfwGetVideoMode(monitor);
    glfwSetWindowAttrib(gw, GLFW_DECORATED, GLFW_FALSE);
    glfwSetWindowPos(gw, 0, 0);
    glfwSetWindowSize(gw, mode->width, mode->height);
}

void GLFWOutputWindow::setWindowed()
{
    GLFWwindow* gw = static_cast<GLFWwindow*>(this->window);
    glfwSetWindowAttrib(gw, GLFW_DECORATED, GLFW_TRUE);
    glfwSetWindowPos(gw, savedX, savedY);
    glfwSetWindowSize(gw, savedW, savedH);
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

GtsKeyTranslator* GLFWOutputWindow::getKeyTranslatorPtr() const
{
    return keyTranslator.get();
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