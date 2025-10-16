#pragma once

#include <memory>
#include <string>
#include <vulkan/vulkan.h>

// config of this class
#include "VulkanContextConfig.h"

// Output window
#include "OutputWindow.hpp"

// vulkan instance includes
#include "VulkanInstanceConfig.h"
#include "VulkanInstance.hpp"

// window surface includes
#include "VulkanSurfaceConfig.h"
#include "VulkanSurface.hpp"
#include "GLFWVulkanSurface.hpp"

// physical device includes
#include "VulkanPhysicalDevice.hpp"
#include "VulkanPhysicalDeviceConfig.h"



// make a class that acts as a vulkan context, as a glue to hold it all together
class VulkanContext 
{
    private:
        VulkanContextConfig config;

        // using unique pointers here seems like a good idea, the context "owns" all of the puzzle pieces
        std::unique_ptr<VulkanInstance> vinstance;
        std::unique_ptr<VulkanSurface> vsurface;
        std::unique_ptr<VulkanPhysicalDevice> vphysicaldevice;
        //std::unique_ptr<VulkanLogicalDevice> vlogicaldevice;

        void setupVkInstance()
        {
            // configure extensions
            // need to rework this for the case of no window being present
            if (this->config.enableValidationLayers) 
            {
                this->config.vulkanInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            // make a new vulkan instance with the config
            VulkanInstanceConfig vInstanceConfig;
            vInstanceConfig.enableSurfaceSupport = true;
            vInstanceConfig.enableValidationLayers = this->config.enableValidationLayers;
            vInstanceConfig.extensions = this->config.vulkanInstanceExtensions;
            vInstanceConfig.appname = this->config.applicationName.c_str();
            vinstance = std::make_unique<VulkanInstance>(vInstanceConfig);
        }

        void setupVkSurface()
        {
            // setup a new window surface with its dedicated config
            // this time we use polymorphism, so we only need to pass the instance we configured before, and let the window class do the
            // rest of the work
            VulkanSurfaceConfig wsConfig;
            wsConfig.instance = vinstance->getInstance();
            vsurface = this->config.outputWindowPtr->createSurface(wsConfig);
        }

        void setupVkPhysicalDevice()
        {
            VulkanPhysicalDeviceConfig physConfig;
            physConfig.vkInstance = vinstance->getInstance();
            physConfig.vkSurface = vsurface->getSurface();
            vphysicaldevice = std::make_unique<VulkanPhysicalDevice>(physConfig);
        }

        void init()
        {
            setupVkInstance();
            setupVkSurface();
            setupVkPhysicalDevice();
        }

    public:
        VulkanContext(VulkanContextConfig config)
        {
            this->config = config;
            this->init();
        }

        // manual destructor to maintain correct order of destruction
        ~VulkanContext()
        {
            vphysicaldevice.reset();
            vsurface.reset();
            vinstance.reset();
        }

        // expose vulkan objects by simply accessing the getters from wrapper classes 
        VkInstance& getInstance() { return vinstance.get()->getInstance(); }
        VkSurfaceKHR& getSurface() { return vsurface.get()->getSurface(); }

        // wont be used in practice, but its the glue that keeps the whole thing together for now :D
        VulkanInstance* getInstanceWrapper() { return vinstance.get(); }
        VulkanSurface* getSurfaceWrapper() { return vsurface.get(); }
        VulkanPhysicalDevice* getPhysicalDeviceWrapper() { return vphysicaldevice.get(); }
        //VkPhysicalDevice& getPhysicalDevice() { return vphysicaldevice.get()->getPhysicalDevice(); }
        //QueueFamilyIndices& getQueueFamilyIndices() { return vphysicaldevice.get()->getQueueFamilyIndices(); }
        //SwapChainSupportDetails& getSwapChainSupportDetails() { return vphysicaldevice.get()->getSwapChainSupportDetails(); }
};
