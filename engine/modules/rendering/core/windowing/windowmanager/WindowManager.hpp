#pragma once

#include "GtsPlatformEventBus.hpp"
#include "WindowManagerConfig.h"
#include "OutputWindowConfig.h"
#include "GLFWOutputWindow.hpp"

class WindowManager
{
    private:
        WindowManagerConfig config;
        GtsPlatformEventBus& eventBus;
        std::unique_ptr<OutputWindow> outputWindow;

        void createWindow()
        {
            switch (config.windowBackend)
            {
                case WindowBackend::GLFW:
                    outputWindow = std::make_unique<GLFWOutputWindow>(config.window, eventBus);
                    break;
            }
        }


    public:
        WindowManager(const WindowManagerConfig& config, GtsPlatformEventBus& eventBus)
            : eventBus(eventBus)
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
};
