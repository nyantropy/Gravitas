#include "VulkanLogicalDevice.hpp"

VulkanLogicalDevice::VulkanLogicalDevice(VulkanInstance* vinstance, VulkanPhysicalDevice* vphysicaldevice, bool enableValidationLayers)
{
    this->vinstance = vinstance;
    this->vphysicaldevice = vphysicaldevice;
    this->enableValidationLayers = enableValidationLayers;

    createLogicalDevice();
    createCommandPool();
}

VulkanLogicalDevice::~VulkanLogicalDevice()
{
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
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
    QueueFamilyIndices indices = vphysicaldevice->getQueueFamilyIndices();

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

    createInfo.enabledExtensionCount = static_cast<uint32_t>(vphysicaldevice->getDeviceExtensions().size());
    createInfo.ppEnabledExtensionNames = vphysicaldevice->getDeviceExtensions().data();

    if (enableValidationLayers) 
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(vinstance->getValidationLayers().size());
        createInfo.ppEnabledLayerNames = vinstance->getValidationLayers().data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(vphysicaldevice->getPhysicalDevice(), &createInfo, nullptr, &device) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void VulkanLogicalDevice::createCommandPool() 
{
    QueueFamilyIndices queueFamilyIndices = vphysicaldevice->getQueueFamilyIndices();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create graphics command pool!");
    }
}
