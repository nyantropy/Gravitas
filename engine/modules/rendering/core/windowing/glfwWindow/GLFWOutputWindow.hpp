#pragma once

#include <GLFW/glfw3.h>

#include "OutputWindow.hpp"
#include "OutputWindowConfig.h"
#include "GLFWKeyTranslator.hpp"

class GLFWOutputWindow : public OutputWindow
{
    public:
        GLFWOutputWindow(OutputWindowConfig config, GtsPlatformEventBus& eventBus);
        ~GLFWOutputWindow();

        bool shouldClose() const override;
        void pollEvents() override;
        void getSize(int& width, int& height) const override;
        void* getWindow() const override;

        void setWindowed()             override;
        void setBorderlessFullscreen() override;
        void setFullscreen()           override;
        void setWindowMode(WindowMode mode) override;
        void setWindowSize(int width, int height) override;
        void applyWindowSettings(int width,
                                 int height,
                                 WindowMode mode,
                                 int monitorIndex,
                                 const std::string& monitorName = {}) override;
        std::vector<GraphicsMonitorInfo> getAvailableMonitors() const override;

    private:
        void init() override;
        GLFWmonitor* resolveMonitor(int monitorIndex) const;
        int resolveMonitorIndex(int monitorIndex, const std::string& monitorName) const;
        std::string monitorNameForIndex(int monitorIndex) const;
        void getMonitorWorkArea(GLFWmonitor* monitor, int& x, int& y, int& width, int& height) const;
        void applyWindowed(GLFWmonitor* monitor, bool centerOnMonitor);
        void applyBorderless(GLFWmonitor* monitor);
        void applyFullscreen(GLFWmonitor* monitor);

        // saved windowed state for restoring after fullscreen
        int savedX = 0, savedY = 0, savedW = 0, savedH = 0;

        //this basically just takes the glfw specific event and directs it to whatever callback we have assigned via
        //setOnWindowResizeCallback()
        //could expand this to account for more than one callback, but i dont think it needs to be done for now
        static void framebufferResizeCallbackStatic(GLFWwindow* window, int width, int height) 
        {
            auto app = reinterpret_cast<GLFWOutputWindow*>(glfwGetWindowUserPointer(window));
            if (app) app->eventBus.emit(GtsWindowResizeEvent{width, height});
        }

        static void onKeyPressedCallbackStatic(GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            auto app = reinterpret_cast<GLFWOutputWindow*>(glfwGetWindowUserPointer(window));
            if (!app) return;

            GtsKey gtsKey = app->keyTranslator->fromPlatformScancode(scancode);
            bool   pressed = !app->keyTranslator->isReleaseAction(action);
            app->eventBus.emit(GtsKeyEvent{gtsKey, pressed, mods});
        }

        static void onMouseButtonCallbackStatic(GLFWwindow* window, int button, int action, int mods)
        {
            auto app = reinterpret_cast<GLFWOutputWindow*>(glfwGetWindowUserPointer(window));
            if (!app) return;

            app->eventBus.emit(GtsMouseButtonEvent{button, action != GLFW_RELEASE, mods});
        }

        static void onCursorPositionCallbackStatic(GLFWwindow* window, double xpos, double ypos)
        {
            auto app = reinterpret_cast<GLFWOutputWindow*>(glfwGetWindowUserPointer(window));
            if (!app) return;

            app->eventBus.emit(GtsCursorPositionEvent{xpos, ypos});
        }

        static void onScrollCallbackStatic(GLFWwindow* window, double xoffset, double yoffset)
        {
            auto app = reinterpret_cast<GLFWOutputWindow*>(glfwGetWindowUserPointer(window));
            if (!app) return;

            app->eventBus.emit(GtsScrollEvent{xoffset, yoffset});
        }
};
