#pragma once

#include <functional>
#include <string>

class GTSOutputWindow 
{
    protected:
        std::function<void(int, int)> resizeCallback;
        std::function<void(int, int, int, int)> onKeyPressedCallback;
        
    public:
        virtual ~GTSOutputWindow() = default;

        virtual void init(int width, int height, const std::string& title, bool enableValidationLayers) = 0;
        virtual void setOnWindowResizeCallback(const std::function<void(int, int)>& callback) = 0;
        virtual void setOnKeyPressedCallback(const std::function<void(int, int, int, int)>& callback) = 0;
        virtual bool shouldClose() const = 0;
        virtual void pollEvents() = 0;
        virtual void getSize(int& width, int& height) const = 0;
        virtual void* getWindow() const = 0;
        virtual std::vector<const char*> getRequiredExtensions() const = 0;
};