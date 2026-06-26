#pragma once

#include <memory>
#include <string>
#include <vector>

#include "EngineConfig.h"
#include "GtsEventTypes.h"
#include "GtsPlatformEventBus.hpp"
#include "SubscriptionToken.hpp"
#include "GraphicsModuleFactory.h"
#include "IGtsGraphicsModule.hpp"
#include "InputManager.hpp"
#include "InputBindingRegistry.h"

// Owns all OS-facing engine subsystems: graphics, windowing, and input.
// GravitasEngine holds one GtsPlatform by value and delegates to it.
class GtsPlatform
{
    public:
        explicit GtsPlatform(const EngineConfig& config)
        {
            inputManager  = std::make_unique<InputManager>();
            bindingRegistry = std::make_unique<InputBindingRegistry>();
            createGraphicsModule(config);
            bindDefaultActions();
        }

        // Snapshot previous frame, poll OS events, then derive action states.
        void beginFrame()
        {
            inputManager->beginFrame();
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

        RuntimeGraphicsSettings getRuntimeGraphicsSettings() const
        {
            return graphics->getRuntimeGraphicsSettings();
        }

        std::vector<GraphicsMonitorInfo> getAvailableMonitors() const
        {
            return graphics->getAvailableMonitors();
        }

        bool applyRuntimeGraphicsSettings(const RuntimeGraphicsSettings& settings)
        {
            return graphics->applyRuntimeGraphicsSettings(settings);
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
        std::unique_ptr<InputBindingRegistry>      bindingRegistry;
        SubscriptionToken                          keyEventToken;
        SubscriptionToken                          mouseButtonEventToken;
        SubscriptionToken                          cursorPositionEventToken;
        SubscriptionToken                          scrollEventToken;

        void createGraphicsModule(const EngineConfig& config)
        {
            graphics = gts::rendering::createGraphicsModule(config.graphics);

            keyEventToken = graphics->getEventBus().subscribe<GtsKeyEvent>([this](const GtsKeyEvent& e)
            {
                inputManager->onKeyEvent(e.key, e.pressed, e.mods);
            });
            mouseButtonEventToken = graphics->getEventBus().subscribe<GtsMouseButtonEvent>([this](const GtsMouseButtonEvent& e)
            {
                inputManager->onMouseButtonEvent(e.button, e.pressed, e.mods);
            });
            cursorPositionEventToken = graphics->getEventBus().subscribe<GtsCursorPositionEvent>([this](const GtsCursorPositionEvent& e)
            {
                inputManager->onCursorPositionEvent(e.x, e.y);
            });
            scrollEventToken = graphics->getEventBus().subscribe<GtsScrollEvent>([this](const GtsScrollEvent& e)
            {
                inputManager->onScrollEvent(e.x, e.y);
            });

            if (config.debugOverlayEnabledByDefault)
                graphics->toggleDebugOverlay();
        }

        void bindDefaultActions()
        {
            bindingRegistry->bind("engine.pause",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::X)},
                                  ActivationMode::Pressed,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.close",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::Escape)},
                                  ActivationMode::Pressed,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.toggle_ui",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::F2)},
                                  ActivationMode::Pressed,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.debug_overlay",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::F3)},
                                  ActivationMode::Pressed,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.debug_overlay_page",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::Tab)},
                                  ActivationMode::Pressed,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.screenshot",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::F12)},
                                  ActivationMode::Pressed,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.tools_toggle",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::F6)},
                                  ActivationMode::Pressed,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.ui_primary",
                                  InputTrigger{InputTrigger::Type::MouseButton, 0},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind(InputBinding{
                                  "engine.tools_select",
                                  InputTrigger{InputTrigger::Type::MouseButton, 0},
                                  ActivationMode::Held,
                                  "engine.tools",
                                  PausePolicy::AlwaysActive,
                                  false});
            bindingRegistry->bind("engine.zoom_in",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::ArrowUp)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.zoom_out",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::ArrowDown)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.orbit_left",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::ArrowLeft)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.orbit_right",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::ArrowRight)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindCameraKeyAction("engine.tools", "engine.tool_camera_yaw_left", GtsKey::Q);
            bindCameraKeyAction("engine.tools", "engine.tool_camera_yaw_right", GtsKey::E);
            bindCameraKeyAction("engine.tools", "engine.tool_camera_pitch_up", GtsKey::ArrowUp);
            bindCameraKeyAction("engine.tools", "engine.tool_camera_pitch_down", GtsKey::ArrowDown);
            bindCameraKeyAction("engine.tools", "engine.tool_camera_forward", GtsKey::W);
            bindCameraKeyAction("engine.tools", "engine.tool_camera_backward", GtsKey::S);
            bindCameraKeyAction("engine.tools", "engine.tool_camera_left", GtsKey::A);
            bindCameraKeyAction("engine.tools", "engine.tool_camera_right", GtsKey::D);
            bindCameraKeyAction("engine.tools", "engine.tool_camera_up", GtsKey::R);
            bindCameraKeyAction("engine.tools", "engine.tool_camera_down", GtsKey::F);
            bindCameraMouseAction("engine.tools", "engine.tool_camera_look");
        }

        void bindCameraKeyAction(const std::string& context,
                                 const std::string& action,
                                 GtsKey key,
                                 ActivationMode mode = ActivationMode::Held)
        {
            bindingRegistry->bind(InputBinding{
                                  action,
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(key)},
                                  mode,
                                  context,
                                  PausePolicy::AlwaysActive,
                                  false});
        }

        void bindCameraMouseAction(const std::string& context,
                                   const std::string& action)
        {
            bindingRegistry->bind(InputBinding{
                                  action,
                                  InputTrigger{InputTrigger::Type::MouseButton, 1},
                                  ActivationMode::Held,
                                  context,
                                  PausePolicy::AlwaysActive,
                                  false});
        }
};
