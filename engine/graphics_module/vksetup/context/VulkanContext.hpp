#pragma once

#include <memory>
#include <string>
#include <vulkan/vulkan.h>

// config of this class
#include "VulkanContextConfig.h"

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

// logical device includes
#include "VulkanLogicalDevice.hpp"
#include "VulkanLogicalDeviceConfig.h"

// swapchain includes
#include "VulkanSwapChain.hpp"
#include "VulkanSwapChainConfig.h"

// make a class that acts as a vulkan context, as a glue to hold it all together
class VulkanContext 
{
    private:
        VulkanContextConfig config;

        // using unique pointers here seems like a good idea, the context "owns" all of the puzzle pieces
        std::unique_ptr<VulkanInstance> vinstance;
        std::unique_ptr<VulkanSurface> vsurface;
        std::unique_ptr<VulkanPhysicalDevice> vphysicaldevice;
        std::unique_ptr<VulkanLogicalDevice> vlogicaldevice;
        std::unique_ptr<VulkanSwapChain> vswapchain;

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
            vInstanceConfig.applicationName = this->config.applicationName.c_str();
            vinstance = std::make_unique<VulkanInstance>(vInstanceConfig);
        }

        void setupVkSurface()
        {
            // setup a new window surface with its dedicated config
            // this time we use polymorphism, so we only need to pass the instance we configured before, and let the window class do the
            // rest of the work
            VulkanSurfaceConfig wsConfig;
            wsConfig.vkInstance = this->getInstance();
            vsurface = this->config.outputWindowPtr->createSurface(wsConfig);
        }

        void setupVkPhysicalDevice()
        {
            VulkanPhysicalDeviceConfig physConfig;
            physConfig.vkInstance = this->getInstance();
            physConfig.vkSurface = this->getSurface();
            vphysicaldevice = std::make_unique<VulkanPhysicalDevice>(physConfig);
        }

        void setupVkLogicalDevice()
        {
            VulkanLogicalDeviceConfig vldConfig;

            // first of all we need the instance, as for pretty much anything in the build order here
            vldConfig.vkInstance = this->getInstance();

            // we need the physical device, as well as its extensions and the queue family indices
            vldConfig.vkPhysicalDevice = this->getPhysicalDevice();
            vldConfig.physicalDeviceExtensions = this->getPhysicalDeviceExtensions();
            vldConfig.queueFamilyIndices = this->getQueueFamilyIndices();

            // vk logical device also has validation layers
            vldConfig.enableValidationLayers = this->config.enableValidationLayers;
            vldConfig.vkInstanceValidationLayers = this->getValidationLayers();

            vlogicaldevice = std::make_unique<VulkanLogicalDevice>(vldConfig);
        }

        void setupVkSwapChain()
        {
            VulkanSwapChainConfig vscConfig;
            vscConfig.vkInstance = this->getInstance();
            vscConfig.vkSurface = this->getSurface();
            vscConfig.vkDevice = this->getDevice();
            vscConfig.swapChainSupportDetails = this->getSwapChainSupportDetails();
            vscConfig.queueFamilyIndices = this->getQueueFamilyIndices();
            vscConfig.outputWindowPtr = this->config.outputWindowPtr;
            vswapchain = std::make_unique<VulkanSwapChain>(vscConfig);
        }

        void init()
        {
            setupVkInstance();
            setupVkSurface();
            setupVkPhysicalDevice();
            setupVkLogicalDevice();
            setupVkSwapChain();
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
            if(vswapchain) vswapchain.reset();
            if(vlogicaldevice) vlogicaldevice.reset();
            if(vphysicaldevice) vphysicaldevice.reset();
            if(vsurface) vsurface.reset();
            if(vinstance) vinstance.reset();
        }

        // expose vulkan objects by simply accessing the getters from wrapper classes
        // ---------------------------------------------------------------------------------
        // related to VulkanInstance
        VkInstance& getInstance() { return vinstance->getInstance(); }
        std::vector<const char*> getValidationLayers() { return vinstance->getValidationLayers(); }

        //related to VulkanSurface
        VkSurfaceKHR& getSurface() { return vsurface->getSurface(); }

        // related to VulkanPhysicalDevice
        VkPhysicalDevice& getPhysicalDevice() { return vphysicaldevice->getPhysicalDevice(); }
        QueueFamilyIndices& getQueueFamilyIndices() { return vphysicaldevice->getQueueFamilyIndices(); }
        SwapChainSupportDetails& getSwapChainSupportDetails() { return vphysicaldevice->getSwapChainSupportDetails(); }
        std::vector<const char*> getPhysicalDeviceExtensions() { return vphysicaldevice->getPhysicalDeviceExtensions(); }

        // related to VulkanLogicalDevice
        VkDevice& getDevice() { return vlogicaldevice->getDevice(); }
        VkQueue& getGraphicsQueue() { return vlogicaldevice->getGraphicsQueue(); }
        VkQueue& getPresentQueue() { return vlogicaldevice->getPresentQueue(); }
        VkCommandPool& getCommandPool() { return vlogicaldevice->getCommandPool(); }

        // related to VulkanSwapChain
        VkSwapchainKHR& getSwapChain() { return vswapchain->getSwapChain(); }
        std::vector<VkImage>& getSwapChainImages() { return vswapchain->getSwapChainImages(); }
        VkFormat& getSwapChainImageFormat() { return vswapchain->getSwapChainImageFormat(); }
        VkExtent2D& getSwapChainExtent() { return vswapchain->getSwapChainExtent(); }
        std::vector<VkImageView>& getSwapChainImageViews() { return vswapchain->getSwapChainImageViews(); }

        // also exposing the concrete wrapper objects as pointers - this is not needed in the final codebase, but is still
        // included for now as an alternative way of accessing lower level vulkan constructs
        // ----------------------------------------------------------------------------------
        VulkanInstance* getInstanceWrapper() { return vinstance.get(); }
        VulkanSurface* getSurfaceWrapper() { return vsurface.get(); }
        VulkanPhysicalDevice* getPhysicalDeviceWrapper() { return vphysicaldevice.get(); }
        VulkanLogicalDevice* getLogicalDeviceWrapper() { return vlogicaldevice.get(); }
        VulkanSwapChain* getSwapChainWrapper() { return vswapchain.get(); }
};
