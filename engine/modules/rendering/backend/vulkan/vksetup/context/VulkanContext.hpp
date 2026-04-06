#pragma once

#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

// config of this class
#include "VulkanContextConfig.h"

// vulkan instance includes
#include "VulkanInstanceConfig.h"
#include "VulkanInstance.hpp"

// window surface includes
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
        const std::vector<VkImage>* frameOutputImages = nullptr;
        const std::vector<VkImageView>* frameOutputImageViews = nullptr;
        VkFormat frameOutputFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D frameOutputExtent{1, 1};
        VkPresentModeKHR frameOutputPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

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
            vInstanceConfig.enableSurfaceSupport = this->config.enableSurfaceSupport;
            vInstanceConfig.enableValidationLayers = this->config.enableValidationLayers;
            vInstanceConfig.extensions = this->config.vulkanInstanceExtensions;
            vInstanceConfig.applicationName = this->config.applicationName.c_str();
            vinstance = std::make_unique<VulkanInstance>(vInstanceConfig);
        }

        void setupVkSurface()
        {
            GLFWwindow* nativeHandle = static_cast<GLFWwindow*>(this->config.outputWindowPtr->getWindow());
            VulkanSurfaceConfig surfConfig;
            surfConfig.vkInstance   = this->getInstance();
            surfConfig.nativeWindow = nativeHandle;
            vsurface = std::make_unique<GLFWVulkanSurface>(surfConfig);
        }

        void setupVkPhysicalDevice()
        {
            VulkanPhysicalDeviceConfig physConfig;
            physConfig.vkInstance = this->getInstance();
            physConfig.vkSurface = this->config.enableSurfaceSupport ? this->getSurface() : VK_NULL_HANDLE;
            physConfig.requirePresentSupport = this->config.enableSurfaceSupport;
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
            vldConfig.requirePresentQueue = this->config.enableSurfaceSupport;

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
            vscConfig.presentModePreference = this->config.presentModePreference;
            vswapchain = std::make_unique<VulkanSwapChain>(vscConfig);
        }

        void init()
        {
            setupVkInstance();
            if (this->config.enableSurfaceSupport)
                setupVkSurface();
            setupVkPhysicalDevice();
            setupVkLogicalDevice();
            if (this->config.enableSurfaceSupport)
                setupVkSwapChain();
        }

    public:
        VulkanContext(VulkanContextConfig config)
        {
            this->config = config;
            this->init();
            if (vswapchain)
            {
                registerFrameOutput(vswapchain->getSwapChainImages(),
                                    vswapchain->getSwapChainImageViews(),
                                    vswapchain->getSwapChainImageFormat(),
                                    vswapchain->getSwapChainExtent(),
                                    vswapchain->getPresentMode());
            }
            else
            {
                frameOutputExtent = {this->config.renderWidth, this->config.renderHeight};
            }
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
        VkPresentModeKHR getPresentMode() const { return vswapchain->getPresentMode(); }
        bool hasSurfaceSupport() const { return config.enableSurfaceSupport; }
        bool isHeadless() const { return config.headless; }

        void registerFrameOutput(const std::vector<VkImage>& images,
                                 const std::vector<VkImageView>& imageViews,
                                 VkFormat format,
                                 VkExtent2D extent,
                                 VkPresentModeKHR presentMode)
        {
            frameOutputImages = &images;
            frameOutputImageViews = &imageViews;
            frameOutputFormat = format;
            frameOutputExtent = extent;
            frameOutputPresentMode = presentMode;
        }

        const std::vector<VkImage>& getFrameOutputImages() { return *frameOutputImages; }
        const std::vector<VkImageView>& getFrameOutputImageViews() { return *frameOutputImageViews; }
        VkFormat getFrameOutputFormat() const { return frameOutputFormat; }
        VkExtent2D getFrameOutputExtent() const { return frameOutputExtent; }
        VkPresentModeKHR getFrameOutputPresentMode() const { return frameOutputPresentMode; }

        // also exposing the concrete wrapper objects as pointers - this is not needed in the final codebase, but is still
        // included for now as an alternative way of accessing lower level vulkan constructs
        // ----------------------------------------------------------------------------------
        VulkanInstance* getInstanceWrapper() { return vinstance.get(); }
        VulkanSurface* getSurfaceWrapper() { return vsurface.get(); }
        VulkanPhysicalDevice* getPhysicalDeviceWrapper() { return vphysicaldevice.get(); }
        VulkanLogicalDevice* getLogicalDeviceWrapper() { return vlogicaldevice.get(); }
        VulkanSwapChain* getSwapChainWrapper() { return vswapchain.get(); }
};
