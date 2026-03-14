#pragma once

#include <functional>
#include <string>
#include <memory>

#include "OutputWindowConfig.h"
#include "GtsEvent.hpp"
#include "GtsKeyEvent.h"
#include "GtsKeyTranslator.hpp"

class OutputWindow
{
    protected:
        explicit OutputWindow(const OutputWindowConfig& config): config(config) {};
        virtual void init() = 0;

        OutputWindowConfig config;
        void* window;
        std::unique_ptr<GtsKeyTranslator> keyTranslator;

    public:

        virtual ~OutputWindow() = default;

        GtsEvent<int, int> onResize;
        GtsEvent<GtsKeyEvent> onKeyPressed;

        // misc methods
        virtual bool shouldClose() const = 0;
        virtual void pollEvents() = 0;
        virtual void getSize(int& width, int& height) const = 0;
        virtual void* getWindow() const = 0;

        float getAspectRatio() const
        {
            int w = 0, h = 0;
            getSize(w, h);
            return h > 0 ? float(w) / float(h) : 1.0f;
        }

        // Borderless fullscreen toggle — saves and restores windowed state.
        // No-op default allows platforms that don't support fullscreen to compile.
        virtual void setWindowed()             {}
        virtual void setBorderlessFullscreen() {}
        virtual void setFullscreen()           {}
};