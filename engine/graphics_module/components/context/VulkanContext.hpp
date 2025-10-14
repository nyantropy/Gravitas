#pragma once

#include <memory>

#include <vulkan/vulkan.h>

#include "VulkanPhysicalDevice.hpp"
#include "VulkanInstance.hpp"
#include "VulkanLogicalDevice.hpp"

// make a class that acts as a vulkan context, as a glue to hold it all together
class VulkanContext {
private:
    // using unique pointers here seems like a good idea, the context "owns" all of the puzzle pieces
    std::unique_ptr<VulkanInstance> vinstance;
    std::unique_ptr<VulkanPhysicalDevice> vphysicaldevice;
    std::unique_ptr<VulkanLogicalDevice> vlogicaldevice;

    void initialize()
    {
        
    }

public:
    VulkanContext();
    ~VulkanContext() = default;  // unique_ptr auto deletes

    // expose vulkan objects by simply accessing the getters from wrapper classes 
    VkInstance& getInstance() { return vinstance.get()->getInstance(); }
    VkPhysicalDevice& getPhysicalDevice() { return vphysicaldevice.get()->getPhysicalDevice(); }
    QueueFamilyIndices& getQueueFamilyIndices() { return vphysicaldevice.get()->getQueueFamilyIndices(); }
    SwapChainSupportDetails& getSwapChainSupportDetails() { return vphysicaldevice.get()->getSwapChainSupportDetails(); }
};
