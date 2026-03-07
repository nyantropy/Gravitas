#pragma once

#include <functional>
#include <string>
#include <memory>

#include <vulkan/vulkan.h>

#include "OutputWindowConfig.h"
#include "VulkanSurfaceConfig.h"
#include "VulkanSurface.hpp"
#include "GtsEvent.hpp"
#include "GtsKeyTranslator.hpp"
#include "InputManager.hpp"

class OutputWindow
{
    protected:
        explicit OutputWindow(const OutputWindowConfig& config): config(config) {};
        virtual void init() = 0;

        OutputWindowConfig config;
        void* window;
        std::unique_ptr<GtsKeyTranslator> keyTranslator;

        // Set by GravitasEngine after both the window and InputManager are created.
        InputManager* inputManager = nullptr;

        // Deliver a raw key event to the InputManager (valid because OutputWindow
        // is declared a friend inside InputManager).
        void processKeyEvent(GtsKey gtskey, bool pressed)
        {
            if (inputManager)
                inputManager->onKeyEvent(gtskey, pressed);
        }

    public:
        void setInputManager(InputManager* mgr) { inputManager = mgr; }

        virtual ~OutputWindow() = default;

        GtsEvent<int, int> onResize;
        GtsEvent<int, int, int, int> onKeyPressed;

        // misc methods
        virtual bool shouldClose() const = 0;
        virtual void pollEvents() = 0;
        virtual void getSize(int& width, int& height) const = 0;
        virtual void* getWindow() const = 0;
        virtual GtsKeyTranslator* getKeyTranslatorPtr() const = 0;
        virtual std::vector<const char*> getRequiredExtensions() const = 0;

        float getAspectRatio() const
        {
            int w = 0, h = 0;
            getSize(w, h);
            return h > 0 ? float(w) / float(h) : 1.0f;
        }

        // Borderless fullscreen toggle — saves and restores windowed state.
        // No-op default allows platforms that don't support fullscreen to compile.
        virtual void setFullscreen() {}
        virtual void setWindowed()   {}

        // important factory pattern for easier surface creation and windowing extensions
        virtual std::unique_ptr<VulkanSurface> createSurface(VulkanSurfaceConfig config) const = 0;
};