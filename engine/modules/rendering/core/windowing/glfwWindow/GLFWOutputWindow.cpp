#include "GLFWOutputWindow.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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

int GLFWOutputWindow::resolveMonitorIndex(int monitorIndex, const std::string& monitorName) const
{
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (count <= 0 || monitors == nullptr)
        return 0;

    if (!monitorName.empty())
    {
        for (int i = 0; i < count; ++i)
        {
            const char* name = glfwGetMonitorName(monitors[i]);
            if (name != nullptr && monitorName == name)
                return i;
        }
    }

    return std::clamp(monitorIndex, 0, count - 1);
}

GLFWmonitor* GLFWOutputWindow::resolveMonitor(int monitorIndex) const
{
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (count <= 0 || monitors == nullptr)
        return glfwGetPrimaryMonitor();

    const int index = std::clamp(monitorIndex, 0, count - 1);
    return monitors[index];
}

std::string GLFWOutputWindow::monitorNameForIndex(int monitorIndex) const
{
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (count <= 0 || monitors == nullptr)
        return {};

    const int index = std::clamp(monitorIndex, 0, count - 1);
    const char* name = glfwGetMonitorName(monitors[index]);
    return name != nullptr ? std::string{name} : std::string{};
}

void GLFWOutputWindow::getMonitorWorkArea(GLFWmonitor* monitor, int& x, int& y, int& width, int& height) const
{
    if (monitor == nullptr)
    {
        x = 0;
        y = 0;
        width = std::max(1, config.width);
        height = std::max(1, config.height);
        return;
    }

    glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);
}

void GLFWOutputWindow::applyWindowed(GLFWmonitor* monitor, bool centerOnMonitor)
{
    GLFWwindow* gw = static_cast<GLFWwindow*>(this->window);
    const int targetWidth = std::max(1, config.width);
    const int targetHeight = std::max(1, config.height);
    glfwSetWindowAttrib(gw, GLFW_DECORATED, GLFW_TRUE);
    glfwSetWindowAttrib(gw, GLFW_RESIZABLE, GLFW_TRUE);

    int workX = 0;
    int workY = 0;
    int workW = targetWidth;
    int workH = targetHeight;
    getMonitorWorkArea(monitor, workX, workY, workW, workH);

    int frameLeft = 0;
    int frameTop = 0;
    int frameRight = 0;
    int frameBottom = 0;
    glfwGetWindowFrameSize(gw, &frameLeft, &frameTop, &frameRight, &frameBottom);

    int x = savedX;
    int y = savedY;
    if (centerOnMonitor)
    {
        x = workX + (workW - (targetWidth + frameLeft + frameRight)) / 2 + frameLeft;
        y = workY + (workH - (targetHeight + frameTop + frameBottom)) / 2 + frameTop;
    }

    const int minX = workX + frameLeft;
    const int minY = workY + frameTop;
    const int maxX = workX + workW - frameRight - targetWidth;
    const int maxY = workY + workH - frameBottom - targetHeight;
    x = std::clamp(x, minX, std::max(minX, maxX));
    y = std::clamp(y, minY, std::max(minY, maxY));

    glfwSetWindowMonitor(gw, nullptr, x, y, targetWidth, targetHeight, 0);
    savedX = x;
    savedY = y;
    savedW = targetWidth;
    savedH = targetHeight;
    config.windowMode = WindowMode::Windowed;
}

void GLFWOutputWindow::applyBorderless(GLFWmonitor* monitor)
{
    GLFWwindow*        gw      = static_cast<GLFWwindow*>(this->window);
    if (monitor == nullptr)
        return;

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (mode == nullptr)
        return;

    if (config.windowMode == WindowMode::Windowed)
    {
        glfwGetWindowPos(gw, &savedX, &savedY);
        glfwGetWindowSize(gw, &savedW, &savedH);
    }
    int monitorX = 0;
    int monitorY = 0;
    glfwGetMonitorPos(monitor, &monitorX, &monitorY);
    glfwSetWindowAttrib(gw, GLFW_DECORATED,    GLFW_FALSE);
    glfwSetWindowAttrib(gw, GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwSetWindowMonitor(gw, monitor, monitorX, monitorY, mode->width, mode->height, mode->refreshRate);
    config.windowMode = WindowMode::BorderlessFullscreen;
}

void GLFWOutputWindow::applyFullscreen(GLFWmonitor* monitor)
{
    GLFWwindow*        gw      = static_cast<GLFWwindow*>(this->window);
    if (monitor == nullptr)
        return;

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (mode == nullptr)
        return;

    if (config.windowMode == WindowMode::Windowed)
    {
        glfwGetWindowPos(gw, &savedX, &savedY);
        glfwGetWindowSize(gw, &savedW, &savedH);
    }

    const int targetWidth = std::max(1, config.width);
    const int targetHeight = std::max(1, config.height);
    glfwSetWindowAttrib(gw, GLFW_DECORATED, GLFW_FALSE);
    glfwSetWindowAttrib(gw, GLFW_AUTO_ICONIFY, GLFW_TRUE);
    glfwSetWindowMonitor(gw, monitor, 0, 0, targetWidth, targetHeight, mode->refreshRate);
    config.windowMode = WindowMode::Fullscreen;
}

std::vector<GraphicsMonitorInfo> GLFWOutputWindow::getAvailableMonitors() const
{
    std::vector<GraphicsMonitorInfo> result;
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    GLFWmonitor* primary = glfwGetPrimaryMonitor();

    for (int i = 0; i < count; ++i)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        int x = 0;
        int y = 0;
        glfwGetMonitorPos(monitors[i], &x, &y);

        GraphicsMonitorInfo info;
        info.index = i;
        const char* name = glfwGetMonitorName(monitors[i]);
        info.name = name != nullptr ? name : ("Monitor " + std::to_string(i + 1));
        info.x = x;
        info.y = y;
        info.width = mode != nullptr ? mode->width : 0;
        info.height = mode != nullptr ? mode->height : 0;
        info.primary = monitors[i] == primary;
        result.push_back(std::move(info));
    }

    if (result.empty())
        result.push_back(GraphicsMonitorInfo{});

    return result;
}

void GLFWOutputWindow::applyWindowSettings(int width,
                                           int height,
                                           WindowMode mode,
                                           int monitorIndex,
                                           const std::string& monitorName)
{
    const WindowMode previousMode = config.windowMode;
    const int previousMonitor = config.monitorIndex;
    config.width = std::max(1, width);
    config.height = std::max(1, height);
    config.monitorIndex = resolveMonitorIndex(monitorIndex, monitorName);
    config.monitorName = monitorNameForIndex(config.monitorIndex);

    GLFWmonitor* monitor = resolveMonitor(config.monitorIndex);
    switch (mode)
    {
        case WindowMode::Windowed:
            applyWindowed(monitor, previousMode != WindowMode::Windowed || previousMonitor != config.monitorIndex);
            break;
        case WindowMode::BorderlessFullscreen:
            applyBorderless(monitor);
            break;
        case WindowMode::Fullscreen:
            applyFullscreen(monitor);
            break;
    }
}

void GLFWOutputWindow::setWindowed()
{
    applyWindowSettings(config.width, config.height, WindowMode::Windowed, config.monitorIndex, config.monitorName);
}

void GLFWOutputWindow::setBorderlessFullscreen()
{
    applyWindowSettings(config.width,
                        config.height,
                        WindowMode::BorderlessFullscreen,
                        config.monitorIndex,
                        config.monitorName);
}

void GLFWOutputWindow::setFullscreen()
{
    applyWindowSettings(config.width, config.height, WindowMode::Fullscreen, config.monitorIndex, config.monitorName);
}

void GLFWOutputWindow::setWindowMode(WindowMode mode)
{
    applyWindowSettings(config.width, config.height, mode, config.monitorIndex, config.monitorName);
}

void GLFWOutputWindow::setWindowSize(int width, int height)
{
    config.width = std::max(1, width);
    config.height = std::max(1, height);
    savedW = config.width;
    savedH = config.height;

    GLFWwindow* gw = static_cast<GLFWwindow*>(this->window);
    if (config.windowMode == WindowMode::Windowed)
    {
        glfwSetWindowSize(gw, config.width, config.height);
        return;
    }

    if (config.windowMode == WindowMode::Fullscreen)
    {
        GLFWmonitor* monitor = glfwGetWindowMonitor(gw);
        if (monitor == nullptr)
            monitor = glfwGetPrimaryMonitor();
        if (monitor == nullptr)
            return;

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        const int refreshRate = mode != nullptr ? mode->refreshRate : GLFW_DONT_CARE;
        glfwSetWindowMonitor(gw, monitor, 0, 0, config.width, config.height, refreshRate);
    }
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
