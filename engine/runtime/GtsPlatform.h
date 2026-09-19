#pragma once

#include <memory>
#include <vector>

#include "GraphicsConfig.h"
#include "GtsEventTypes.h"
#include "GtsPlatformEventBus.hpp"
#include "SubscriptionToken.hpp"
#include "GraphicsBackendRegistry.h"
#include "IGtsGraphicsModule.hpp"
#include "InputWriter.hpp"
#include "InputBindingRegistry.h"

// Owns all OS-facing engine subsystems: graphics, windowing, and input.
// GravitasEngine holds one GtsPlatform by value and delegates to it.
class GtsPlatform
{
    public:
        GtsPlatform(const GraphicsConfig& config,
                    const gts::rendering::GraphicsBackendRegistry& graphicsBackendRegistry)
            : graphics(graphicsBackendRegistry.create(config))
            , inputManager(std::make_unique<InputManager>())
            , inputWriter(*inputManager)
            , bindingRegistry(std::make_unique<InputBindingRegistry>())
        {
            connectInputEvents();
        }

        // Snapshot previous frame, poll OS events, then derive action states.
        void beginFrame()
        {
            inputWriter.beginFrame();
            graphics->pollWindowEvents();
            graphics->getEventBus().dispatch();
            bindingRegistry->update(InputSnapshot{inputManager.get()});
        }

        void shutdown()
        {
            graphics->shutdown();
        }

        bool isWindowOpen() const
        {
            return graphics->isWindowOpen();
        }

        float getAspectRatio() const
        {
            return graphics->getAspectRatio();
        }

        void getViewportSize(int& width, int& height) const
        {
            graphics->getViewportSize(width, height);
        }

        GraphicsSettings getRequestedGraphicsSettings() const
        {
            return graphics->getRequestedGraphicsSettings();
        }

        std::vector<GraphicsMonitorInfo> getAvailableMonitors() const
        {
            return graphics->getAvailableMonitors();
        }

        GraphicsSettingsApplyResult applyGraphicsSettings(const GraphicsSettings& settings)
        {
            return graphics->applyGraphicsSettings(settings);
        }

        IResourceProvider* getResourceProvider()
        {
            return graphics->getResourceProvider();
        }

        InputBindingRegistry* getInputBindingRegistry()
        {
            return bindingRegistry.get();
        }

        // Exposes the graphics module for render calls.
        IGtsGraphicsModule* getGraphics()
        {
            return graphics.get();
        }

        void toggleDebugOverlay()
        {
            graphics->toggleDebugOverlay();
        }

        void cycleDebugOverlayPage()
        {
            graphics->cycleDebugOverlayPage();
        }

        void waitForGraphicsIdle()
        {
            graphics->waitIdle();
        }

        void dispatchGraphicsEvents()
        {
            graphics->getEventBus().dispatch();
        }

    private:
        std::unique_ptr<IGtsGraphicsModule>        graphics;
        std::unique_ptr<InputManager>              inputManager;
        InputWriter                               inputWriter;
        std::unique_ptr<InputBindingRegistry>      bindingRegistry;
        SubscriptionToken                          keyEventToken;
        SubscriptionToken                          mouseButtonEventToken;
        SubscriptionToken                          cursorPositionEventToken;
        SubscriptionToken                          scrollEventToken;

        void connectInputEvents()
        {
            keyEventToken = graphics->getEventBus().subscribe<GtsKeyEvent>([this](const GtsKeyEvent& e)
            {
                inputWriter.onKeyEvent(e.key, e.pressed, e.mods);
            });
            mouseButtonEventToken = graphics->getEventBus().subscribe<GtsMouseButtonEvent>([this](const GtsMouseButtonEvent& e)
            {
                inputWriter.onMouseButtonEvent(e.button, e.pressed, e.mods);
            });
            cursorPositionEventToken = graphics->getEventBus().subscribe<GtsCursorPositionEvent>([this](const GtsCursorPositionEvent& e)
            {
                inputWriter.onCursorPositionEvent(e.x, e.y);
            });
            scrollEventToken = graphics->getEventBus().subscribe<GtsScrollEvent>([this](const GtsScrollEvent& e)
            {
                inputWriter.onScrollEvent(e.x, e.y);
            });
        }
};
