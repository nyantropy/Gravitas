#pragma once

#include <GLFW/glfw3.h>

#include "GTSOutputWindow.hpp"

class GTSGLFWOutputWindow : public GTSOutputWindow 
{
    public:
        GTSGLFWOutputWindow() : window(nullptr), resizeCallback(nullptr) {}
        ~GTSGLFWOutputWindow();

        void init(int width, int height, const std::string& title) override;
        void setOnWindowResizeCallback(void (*callback)(int, int)) override;
        bool shouldClose() const override;
        void pollEvents() override;
        void getSize(int& width, int& height) const override;
        void* getWindow() const override;

    private:
        //this basically just takes the glfw specific event and directs it to whatever callback we have assigned via
        //setOnWindowResizeCallback()
        //could expand this to account for more than one callback, but i dont think it needs to be done for now
        static void framebufferResizeCallbackStatic(GLFWwindow* window, int width, int height) 
        {
            auto app = reinterpret_cast<GTSGLFWOutputWindow*>(glfwGetWindowUserPointer(window));
            if (app->resizeCallback) 
            {
                app->resizeCallback(width, height);
            }
        }

        GLFWwindow* window;
        void (*resizeCallback)(int, int);
};