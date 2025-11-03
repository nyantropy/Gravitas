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
#include "gtsinput.h"

class OutputWindow 
{
    protected:
        explicit OutputWindow(const OutputWindowConfig& config): config(config) {};
        virtual void init() = 0;

        OutputWindowConfig config;
        void* window;
        std::unique_ptr<GtsKeyTranslator> keyTranslator;

        // process the key event here as a friend of the input manager
        void processKeyEvent(GtsKey gtskey, bool pressed)
        {
            gtsinput::getInputManager()->onKeyEvent(gtskey, pressed);
        }

        
    public:
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
        
        // important factory pattern for easier surface creation and windowing extensions
        virtual std::unique_ptr<VulkanSurface> createSurface(VulkanSurfaceConfig config) const = 0;
};