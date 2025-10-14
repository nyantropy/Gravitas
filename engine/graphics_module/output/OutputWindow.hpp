#pragma once

#include <functional>
#include <string>

#include "OutputWindowConfig.h"

class OutputWindow 
{
    protected:
        std::function<void(int, int)> resizeCallback;
        std::function<void(int, int, int, int)> onKeyPressedCallback;

        explicit OutputWindow(const OutputWindowConfig& config): config(config) {};
        virtual void init() = 0;

        OutputWindowConfig config;
        
    public:
        virtual ~OutputWindow() = default;

        virtual void setOnWindowResizeCallback(const std::function<void(int, int)>& callback) = 0;
        virtual void setOnKeyPressedCallback(const std::function<void(int, int, int, int)>& callback) = 0;
        virtual bool shouldClose() const = 0;
        virtual void pollEvents() = 0;
        virtual void getSize(int& width, int& height) const = 0;
        virtual void* getWindow() const = 0;
        virtual std::vector<const char*> getRequiredExtensions() const = 0;
};