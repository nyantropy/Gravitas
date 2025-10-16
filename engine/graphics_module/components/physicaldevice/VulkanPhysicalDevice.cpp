#include "VulkanPhysicalDevice.hpp"

VulkanPhysicalDevice::VulkanPhysicalDevice(VulkanInstance* vinstance, VulkanSurface* vsurface)
{
    this->vinstance = vinstance;
    this->vsurface = vsurface;
    this->pickPhysicalDevice();
}

VulkanPhysicalDevice::~VulkanPhysicalDevice()
{
}

VkPhysicalDevice& VulkanPhysicalDevice::getPhysicalDevice()
{
    return physicalDevice;
}

QueueFamilyIndices& VulkanPhysicalDevice::getQueueFamilyIndices()
{
    return queueFamilyIndices;
}

SwapChainSupportDetails& VulkanPhysicalDevice::getSwapChainSupportDetails()
{
    return swapChainSupportDetails;
}

const std::vector<const char*>& VulkanPhysicalDevice::getDeviceExtensions()
{
    return deviceExtensions;
}

void VulkanPhysicalDevice::pickPhysicalDevice() 
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vinstance->getInstance(), &deviceCount, nullptr);

    if (deviceCount == 0) 
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vinstance->getInstance(), &deviceCount, devices.data());

    for (const auto& device : devices) 
    {
        QueueFamilyIndices indices = findQueueFamilies(device);

        if (isDeviceSuitable(device, indices)) 
        {
            physicalDevice = device;
            queueFamilyIndices = indices;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) 
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool VulkanPhysicalDevice::isDeviceSuitable(VkPhysicalDevice device, QueueFamilyIndices indices) 
{
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) 
    {
        swapChainSupportDetails = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupportDetails.formats.empty() && !swapChainSupportDetails.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate  && supportedFeatures.samplerAnisotropy;
}

SwapChainSupportDetails VulkanPhysicalDevice::querySwapChainSupport(VkPhysicalDevice device) 
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, vsurface->getSurface(), &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, vsurface->getSurface(), &formatCount, nullptr);

    if (formatCount != 0) 
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, vsurface->getSurface(), &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, vsurface->getSurface(), &presentModeCount, nullptr);

    if (presentModeCount != 0) 
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, vsurface->getSurface(), &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool VulkanPhysicalDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) 
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) 
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices VulkanPhysicalDevice::findQueueFamilies(VkPhysicalDevice device) 
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) 
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) 
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vsurface->getSurface(), &presentSupport);

        if (presentSupport) 
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) 
        {
            break;
        }

        i++;
    }

    return indices;
}

uint32_t VulkanPhysicalDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) 
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) 
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) 
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}