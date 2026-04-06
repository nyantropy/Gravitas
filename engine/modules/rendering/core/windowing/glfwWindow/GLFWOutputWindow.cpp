#include "GLFWOutputWindow.hpp"

GLFWOutputWindow::GLFWOutputWindow(OutputWindowConfig config, GtsEventBus& eventBus)
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
    glfwSetWindowAttrib(gw, GLFW_DECORATED, GLFW_TRUE);
    glfwSetWindowAttrib(gw, GLFW_RESIZABLE, GLFW_TRUE);
    glfwSetWindowMonitor(gw, nullptr, savedX, savedY, config.width, config.height, 0);
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
