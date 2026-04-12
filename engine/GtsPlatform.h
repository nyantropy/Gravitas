#pragma once

#include <memory>

#include "EngineConfig.h"
#include "GtsPlatformEventBus.hpp"
#include "SubscriptionToken.hpp"
#include "IGtsGraphicsModule.hpp"
#include "VulkanGraphics.hpp"
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

        void createGraphicsModule(const EngineConfig& config)
        {
            switch (config.graphics.backend)
            {
                case GraphicsBackend::Vulkan:
                    graphics = std::make_unique<VulkanGraphics>(config.graphics);
                    break;
            }

            keyEventToken = graphics->getEventBus().subscribe<GtsKeyEvent>([this](const GtsKeyEvent& e)
            {
                inputManager->onKeyEvent(e.key, e.pressed, e.mods);
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
            bindingRegistry->bind("engine.screenshot",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::F12)},
                                  ActivationMode::Pressed,
                                  "",
                                  PausePolicy::AlwaysActive);
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
            bindingRegistry->bind("engine.debug_camera_toggle",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::F4)},
                                  ActivationMode::Pressed,
                                  "",
                                  PausePolicy::AlwaysActive);
            bindingRegistry->bind("engine.debug_camera_yaw_left",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::Q)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::Gameplay);
            bindingRegistry->bind("engine.debug_camera_yaw_right",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::E)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::Gameplay);
            bindingRegistry->bind("engine.debug_camera_pitch_up",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::ArrowUp)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::Gameplay);
            bindingRegistry->bind("engine.debug_camera_pitch_down",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::ArrowDown)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::Gameplay);
            bindingRegistry->bind("engine.debug_camera_forward",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::W)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::Gameplay);
            bindingRegistry->bind("engine.debug_camera_backward",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::S)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::Gameplay);
            bindingRegistry->bind("engine.debug_camera_left",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::A)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::Gameplay);
            bindingRegistry->bind("engine.debug_camera_right",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::D)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::Gameplay);
            bindingRegistry->bind("engine.debug_camera_up",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::R)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::Gameplay);
            bindingRegistry->bind("engine.debug_camera_down",
                                  InputTrigger{InputTrigger::Type::Key, static_cast<int>(GtsKey::F)},
                                  ActivationMode::Held,
                                  "",
                                  PausePolicy::Gameplay);
        }
};
