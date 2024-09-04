#include "VulkanInstanceFactory.hpp"

VulkanInstanceFactory& VulkanInstanceFactory::enableValidationLayers(bool enable) 
{
    validationLayersEnabled = enable;
    return *this;
}

VulkanInstanceFactory& VulkanInstanceFactory::enableSurfaceSupport(bool enable) 
{
    surfaceSupportEnabled = enable;
    return *this;
}

VulkanInstance* VulkanInstanceFactory::createInstance(GTSOutputWindow* vwindow) 
{
    std::vector<const char*> extensions;

    if (surfaceSupportEnabled && vwindow) 
    {
        extensions = vwindow->getRequiredExtensions();
    }

    if (validationLayersEnabled) 
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return new VulkanInstance(validationLayersEnabled, extensions);
}
