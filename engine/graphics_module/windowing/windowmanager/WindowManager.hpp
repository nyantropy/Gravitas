#pragma once

#include "WindowManagerConfig.h"
#include "OutputWindowConfig.h"
#include "GLFWOutputWindow.hpp"

class WindowManager
{
    private:
        WindowManagerConfig config;
        std::unique_ptr<OutputWindow> outputWindow;

        // simply creates our glfw window for now, a very simple wrap to make the base code more readable, and more easily handle resource encapsulation and destruction
        void createWindow()
        {
            OutputWindowConfig owConfig;
            owConfig.enableValidationLayers = config.enableValidationLayers;
            owConfig.width = config.windowWidth;
            owConfig.height = config.windowHeight;
            owConfig.title = config.windowTitle;

            outputWindow = std::make_unique<GLFWOutputWindow>(owConfig);

            // // Subscribe to window events
            // outputWindow->onResize.subscribe(
            //     [this](int width, int height) 
            //     {
            //         //framebufferResized = true;
            //     });

            // outputWindow->onKeyPressed.subscribe(
            //     [this](int key, int scancode, int action, int mods) 
            //     {
            //         //onKeyPressed.notify(key, scancode, action, mods);
            //     });
        }


    public:
        WindowManager(const WindowManagerConfig& config)
        {
            this->config = config;
            createWindow();
        }

        ~WindowManager()
        {
            if (outputWindow) outputWindow.reset();
        }

        OutputWindow* getOutputWindow()
        {
            return outputWindow.get();
        }

        GtsEvent<int, int>& onResize() { return outputWindow->onResize; }
        GtsEvent<int, int, int, int>& onKeyPressed() { return outputWindow->onKeyPressed; }
};