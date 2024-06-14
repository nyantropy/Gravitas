#pragma once

#include <string>

class GTSOutputWindow 
{
    public:
        virtual ~GTSOutputWindow() = default;

        virtual void init(int width, int height, const std::string& title) = 0;
        virtual void setOnWindowResizeCallback(void (*callback)(int, int)) = 0;
        virtual bool shouldClose() const = 0;
        virtual void pollEvents() = 0;
        virtual void getSize(int& width, int& height) const = 0;
        virtual void* getWindow() const = 0;
};