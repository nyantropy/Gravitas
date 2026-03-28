#pragma once

#include <GLFW/glfw3.h>

#include "OutputWindow.hpp"
#include "OutputWindowConfig.h"
#include "GLFWKeyTranslator.hpp"

class GLFWOutputWindow : public OutputWindow
{
    public:
        GLFWOutputWindow(OutputWindowConfig config);
        ~GLFWOutputWindow();

        bool shouldClose() const override;
        void pollEvents() override;
        void getSize(int& width, int& height) const override;
        void* getWindow() const override;

        void setWindowed()             override;
        void setBorderlessFullscreen() override;
        void setFullscreen()           override;

    private:
        void init() override;

        // saved windowed state for restoring after fullscreen
        int savedX = 0, savedY = 0, savedW = 0, savedH = 0;

        //this basically just takes the glfw specific event and directs it to whatever callback we have assigned via
        //setOnWindowResizeCallback()
        //could expand this to account for more than one callback, but i dont think it needs to be done for now
        static void framebufferResizeCallbackStatic(GLFWwindow* window, int width, int height) 
        {
            auto app = reinterpret_cast<GLFWOutputWindow*>(glfwGetWindowUserPointer(window));
            if (app) app->onResize.notify(width, height);
        }

        static void onKeyPressedCallbackStatic(GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            auto app = reinterpret_cast<GLFWOutputWindow*>(glfwGetWindowUserPointer(window));
            if (!app) return;

            GtsKey gtsKey = app->keyTranslator->fromPlatformScancode(scancode);
            bool   pressed = !app->keyTranslator->isReleaseAction(action);
            app->onKeyPressed.notify(GtsKeyEvent{gtsKey, pressed});
        }
};