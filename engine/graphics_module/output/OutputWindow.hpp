#pragma once

#include <functional>
#include <string>
#include <memory>

#include <vulkan/vulkan.h>

#include "OutputWindowConfig.h"
#include "WindowSurfaceConfig.h"
#include "WindowSurface.hpp"

class OutputWindow 
{
    protected:
        std::function<void(int, int)> resizeCallback;
        std::function<void(int, int, int, int)> onKeyPressedCallback;

        explicit OutputWindow(const OutputWindowConfig& config): config(config) {};
        virtual void init() = 0;

        OutputWindowConfig config;
        void* window;
        
    public:
        virtual ~OutputWindow() = default;

        // two events: window resize and keys pressed
        virtual void setOnWindowResizeCallback(const std::function<void(int, int)>& callback) = 0;
        virtual void setOnKeyPressedCallback(const std::function<void(int, int, int, int)>& callback) = 0;

        // misc methods
        virtual bool shouldClose() const = 0;
        virtual void pollEvents() = 0;
        virtual void getSize(int& width, int& height) const = 0;
        virtual void* getWindow() const = 0;
        virtual std::vector<const char*> getRequiredExtensions() const = 0;
        
        // important factory pattern for easier surface creation and windowing extensions
        virtual std::unique_ptr<WindowSurface> createSurface(WindowSurfaceConfig config) const = 0;
};