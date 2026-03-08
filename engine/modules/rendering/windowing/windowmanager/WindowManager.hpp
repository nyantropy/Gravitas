#pragma once

#include "WindowManagerConfig.h"
#include "OutputWindowConfig.h"
#include "GLFWOutputWindow.hpp"

class WindowManager
{
    private:
        WindowManagerConfig config;
        std::unique_ptr<OutputWindow> outputWindow;

        void createWindow()
        {
            OutputWindowConfig owConfig;
            owConfig.enableValidationLayers = config.enableValidationLayers;
            owConfig.width                  = config.windowWidth;
            owConfig.height                 = config.windowHeight;
            owConfig.title                  = config.windowTitle;
            owConfig.borderlessFullscreen   = config.borderlessFullscreen;

            switch (config.windowBackend)
            {
                case WindowBackend::GLFW:
                    outputWindow = std::make_unique<GLFWOutputWindow>(owConfig);
                    break;
            }
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

        OutputWindow* getOutputWindow() const
        {
            return outputWindow.get();
        }

        GtsEvent<int, int>& onResize() { return outputWindow->onResize; }
        GtsEvent<GtsKeyEvent>& onKeyPressed() { return outputWindow->onKeyPressed; }
};