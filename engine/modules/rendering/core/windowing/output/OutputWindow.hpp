#pragma once

#include <functional>
#include <string>
#include <memory>
#include <vector>

#include "GtsPlatformEventBus.hpp"
#include "GtsEventTypes.h"
#include "OutputWindowConfig.h"
#include "GraphicsMonitorInfo.h"
#include "GtsKeyEvent.h"
#include "GtsKeyTranslator.hpp"

class OutputWindow
{
    protected:
        explicit OutputWindow(const OutputWindowConfig& config, GtsPlatformEventBus& eventBus)
            : config(config), eventBus(eventBus) {};
        virtual void init() = 0;

        OutputWindowConfig config;
        void* window;
        std::unique_ptr<GtsKeyTranslator> keyTranslator;
        GtsPlatformEventBus& eventBus;

    public:

        virtual ~OutputWindow() = default;

        // misc methods
        virtual bool shouldClose() const = 0;
        virtual void pollEvents() = 0;
        virtual void getSize(int& width, int& height) const = 0;
        virtual void* getWindow() const = 0;
        virtual WindowMode getWindowMode() const { return config.windowMode; }
        virtual OutputWindowConfig getConfig() const { return config; }
        virtual std::vector<GraphicsMonitorInfo> getAvailableMonitors() const { return {}; }

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
        virtual void setWindowMode(WindowMode mode)
        {
            switch (mode)
            {
                case WindowMode::Windowed:             setWindowed();             break;
                case WindowMode::BorderlessFullscreen: setBorderlessFullscreen(); break;
                case WindowMode::Fullscreen:           setFullscreen();           break;
            }
        }
        virtual void setWindowSize(int width, int height)
        {
            config.width = width;
            config.height = height;
        }
        virtual void applyWindowSettings(int width,
                                         int height,
                                         WindowMode mode,
                                         int monitorIndex,
                                         const std::string& monitorName = {})
        {
            setWindowSize(width, height);
            config.monitorIndex = monitorIndex;
            config.monitorName = monitorName;
            setWindowMode(mode);
        }
};
