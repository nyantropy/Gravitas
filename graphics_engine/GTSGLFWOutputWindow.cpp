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

void GTSGLFWOutputWindow::init(int width, int height, const std::string& title) 
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallbackStatic);
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