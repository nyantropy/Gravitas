#include "GLFWOutputWindow.hpp"

#include <algorithm>

GLFWOutputWindow::GLFWOutputWindow(OutputWindowConfig config, GtsPlatformEventBus& eventBus)
    : OutputWindow(config, eventBus)
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
    this->window = glfwCreateWindow(
        config.width, config.height, config.title.c_str(), nullptr, nullptr);

    glfwSetWindowUserPointer(static_cast<GLFWwindow*>(this->window), this);
    glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(this->window), framebufferResizeCallbackStatic);
    glfwSetKeyCallback(static_cast<GLFWwindow*>(this->window), onKeyPressedCallbackStatic);
    glfwSetMouseButtonCallback(static_cast<GLFWwindow*>(this->window), onMouseButtonCallbackStatic);
    glfwSetCursorPosCallback(static_cast<GLFWwindow*>(this->window), onCursorPositionCallbackStatic);
    glfwSetScrollCallback(static_cast<GLFWwindow*>(this->window), onScrollCallbackStatic);

    switch (config.windowMode)
    {
        case WindowMode::BorderlessFullscreen: setBorderlessFullscreen(); break;
        case WindowMode::Fullscreen:           setFullscreen();           break;
        case WindowMode::Windowed:             setWindowed();             break;
    }
}

void GLFWOutputWindow::setWindowed()
{
    GLFWwindow* gw = static_cast<GLFWwindow*>(this->window);
    const int targetWidth = std::max(1, config.width);
    const int targetHeight = std::max(1, config.height);
    glfwSetWindowAttrib(gw, GLFW_DECORATED, GLFW_TRUE);
    glfwSetWindowAttrib(gw, GLFW_RESIZABLE, GLFW_TRUE);
    glfwSetWindowMonitor(gw, nullptr, savedX, savedY, targetWidth, targetHeight, 0);
    config.windowMode = WindowMode::Windowed;
}

void GLFWOutputWindow::setBorderlessFullscreen()
{
    GLFWwindow*        gw      = static_cast<GLFWwindow*>(this->window);
    GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode    = glfwGetVideoMode(monitor);
    if (config.windowMode == WindowMode::Windowed)
    {
        glfwGetWindowPos(gw, &savedX, &savedY);
        glfwGetWindowSize(gw, &savedW, &savedH);
    }
    glfwSetWindowAttrib(gw, GLFW_DECORATED,    GLFW_FALSE);
    glfwSetWindowAttrib(gw, GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwSetWindowMonitor(gw, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    config.windowMode = WindowMode::BorderlessFullscreen;
}

void GLFWOutputWindow::setFullscreen()
{
    GLFWwindow*        gw      = static_cast<GLFWwindow*>(this->window);
    GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode    = glfwGetVideoMode(monitor);
    if (config.windowMode == WindowMode::Windowed)
    {
        glfwGetWindowPos(gw, &savedX, &savedY);
        glfwGetWindowSize(gw, &savedW, &savedH);
    }
    glfwSetWindowMonitor(gw, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    config.windowMode = WindowMode::Fullscreen;
}

void GLFWOutputWindow::setWindowMode(WindowMode mode)
{
    switch (mode)
    {
        case WindowMode::Windowed:             setWindowed();             break;
        case WindowMode::BorderlessFullscreen: setBorderlessFullscreen(); break;
        case WindowMode::Fullscreen:           setFullscreen();           break;
    }
}

void GLFWOutputWindow::setWindowSize(int width, int height)
{
    config.width = std::max(1, width);
    config.height = std::max(1, height);
    savedW = config.width;
    savedH = config.height;

    if (config.windowMode == WindowMode::Windowed)
        glfwSetWindowSize(static_cast<GLFWwindow*>(this->window), config.width, config.height);
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
