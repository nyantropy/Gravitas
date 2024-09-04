#include "VulkanInstance.hpp"
#include "GTSOutputWindow.hpp"

#include <vulkan/vulkan.h>

class VulkanInstanceFactory 
{
    public:
        VulkanInstanceFactory& enableValidationLayers(bool enable);
        VulkanInstanceFactory& enableSurfaceSupport(bool enable);
        VulkanInstance* createInstance(GTSOutputWindow* vwindow = nullptr);

    private:
        bool validationLayersEnabled = false;
        bool surfaceSupportEnabled = false;
};