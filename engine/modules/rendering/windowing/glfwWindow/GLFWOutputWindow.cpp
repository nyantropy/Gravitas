#include "GLFWOutputWindow.hpp"

#include <algorithm>

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
        int           monitorCount = 0;
        GLFWmonitor** monitors     = glfwGetMonitors(&monitorCount);
        GLFWmonitor*  monitor      = monitors[std::clamp(config.monitorIndex, 0, monitorCount - 1)];
        const GLFWvidmode* mode    = glfwGetVideoMode(monitor);
        glfwWindowHint(GLFW_RED_BITS,     mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS,   mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS,    mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        glfwWindowHint(GLFW_DECORATED,    GLFW_FALSE);
        this->window = glfwCreateWindow(mode->width, mode->height, config.title.c_str(), monitor, nullptr);
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