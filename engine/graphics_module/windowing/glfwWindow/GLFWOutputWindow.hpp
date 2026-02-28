#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <memory>

#include "OutputWindow.hpp"
#include "OutputWindowConfig.h"

#include "VulkanSurface.hpp"
#include "GLFWVulkanSurface.hpp"
#include "VulkanSurfaceConfig.h"
#include "GLFWKeyTranslator.hpp"

class GLFWOutputWindow : public OutputWindow 
{
    public:
        GLFWOutputWindow(OutputWindowConfig config);
        ~GLFWOutputWindow();

        bool shouldClose() const override;
        void pollEvents() override;
        void getSize(int& width, int& height) const override;
        void* getWindow() const override;
        std::vector<const char*> getRequiredExtensions() const override;

        GtsKeyTranslator* getKeyTranslatorPtr() const override;
        std::unique_ptr<VulkanSurface> createSurface(VulkanSurfaceConfig config) const override;

    private:
        void init() override;

        //this basically just takes the glfw specific event and directs it to whatever callback we have assigned via
        //setOnWindowResizeCallback()
        //could expand this to account for more than one callback, but i dont think it needs to be done for now
        static void framebufferResizeCallbackStatic(GLFWwindow* window, int width, int height) 
        {
            auto app = reinterpret_cast<GLFWOutputWindow*>(glfwGetWindowUserPointer(window));
            if (app) app->onResize.notify(width, height);
        }

        static void onKeyPressedCallbackStatic(GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            auto app = reinterpret_cast<GLFWOutputWindow*>(glfwGetWindowUserPointer(window));

            // if the window does not exist, simply return
            if (!app)
            {
                return;
            }

            // send the translated key into the input manager (new approach)
            GtsKey gtskey = app->getKeyTranslatorPtr()->fromPlatformKey(key);
            app->processKeyEvent(gtskey, action != GLFW_RELEASE);

            // notify our own event (old approach)
            app->onKeyPressed.notify(key, scancode, action, mods);
        }
};