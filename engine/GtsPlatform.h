#pragma once

#include <memory>

#include "EngineConfig.h"
#include "IGtsGraphicsModule.hpp"
#include "VulkanGraphics.hpp"
#include "InputManager.hpp"
#include "InputActionManager.hpp"
#include "PauseFilteredInputSource.hpp"
#include "GtsAction.h"

// Owns all OS-facing engine subsystems: graphics, windowing, and input.
// GravitasEngine holds one GtsPlatform by value and delegates to it.
class GtsPlatform
{
    public:
        explicit GtsPlatform(const EngineConfig& config)
        {
            inputManager  = std::make_unique<InputManager>();
            actionManager = std::make_unique<InputActionManager<GtsAction>>();
            filteredInputSource.setSource(*inputManager);
            createGraphicsModule(config);
            bindDefaultActions();
        }

        // Snapshot previous frame, poll OS events, then derive action states.
        void beginFrame()
        {
            inputManager->beginFrame();
            graphics->pollWindowEvents();
            actionManager->update(*inputManager);
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

        // Returns the pause-gated input source exposed to simulation-coupled
        // controller systems. Engine-level action queries use getActionManager().
        IInputSource* getInputSource()
        {
            return &filteredInputSource;
        }

        InputActionManager<GtsAction>* getActionManager()
        {
            return actionManager.get();
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

        // Gates all key queries through the filtered input source to false
        // while the simulation is paused.
        void setSimulationPaused(bool paused)
        {
            filteredInputSource.setSimulationPaused(paused);
        }

    private:
        std::unique_ptr<IGtsGraphicsModule>        graphics;
        std::unique_ptr<InputManager>              inputManager;
        std::unique_ptr<InputActionManager<GtsAction>> actionManager;
        PauseFilteredInputSource                   filteredInputSource;

        void createGraphicsModule(const EngineConfig& config)
        {
            switch (config.graphics.backend)
            {
                case GraphicsBackend::Vulkan:
                    graphics = std::make_unique<VulkanGraphics>(config.graphics);
                    break;
            }

            graphics->onKeyPressed().subscribe([this](const GtsKeyEvent& e)
            {
                inputManager->onKeyEvent(e.key, e.pressed);
            });

            if (config.debugOverlayEnabledByDefault)
                graphics->toggleDebugOverlay();
        }

        void bindDefaultActions()
        {
            actionManager->bind(GtsAction::TogglePause,       GtsKey::X);
            actionManager->bind(GtsAction::CloseApplication,  GtsKey::Escape);
            actionManager->bind(GtsAction::ToggleUI,          GtsKey::F2);
            actionManager->bind(GtsAction::DebugLayerToggle,  GtsKey::F3);
            actionManager->bind(GtsAction::Screenshot,        GtsKey::F12);
        }
};
