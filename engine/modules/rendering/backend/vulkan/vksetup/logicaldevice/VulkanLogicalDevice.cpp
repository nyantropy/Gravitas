#include "VulkanLogicalDevice.hpp"

VulkanLogicalDevice::VulkanLogicalDevice(VulkanLogicalDeviceConfig config)
{
    this->config = config;
    createLogicalDevice();
    createCommandPool();
}

// increased robustness of destructor here
VulkanLogicalDevice::~VulkanLogicalDevice()
{
    vkDeviceWaitIdle(device);

    if (commandPool != VK_NULL_HANDLE) 
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }

    if (device != VK_NULL_HANDLE) 
    {
        vkDestroyDevice(device, nullptr);
    }
}

VkDevice& VulkanLogicalDevice::getDevice()
{
    return device;
}

VkQueue& VulkanLogicalDevice::getGraphicsQueue()
{
    return graphicsQueue;
}

VkQueue& VulkanLogicalDevice::getPresentQueue()
{
    return presentQueue;
}

VkCommandPool& VulkanLogicalDevice::getCommandPool()
{
    return commandPool;
}


void VulkanLogicalDevice::createLogicalDevice() 
{
    QueueFamilyIndices indices = this->config.queueFamilyIndices;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) 
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(this->config.physicalDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = this->config.physicalDeviceExtensions.data();

    if (this->config.enableValidationLayers) 
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(this->config.vkInstanceValidationLayers.size());
        createInfo.ppEnabledLayerNames = this->config.vkInstanceValidationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(this->config.vkPhysicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void VulkanLogicalDevice::createCommandPool() 
{
    QueueFamilyIndices queueFamilyIndices = this->config.queueFamilyIndices;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create graphics command pool!");
    }
}
