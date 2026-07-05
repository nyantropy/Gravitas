#pragma once

#include <cstdint>
#include <string>
#include <vulkan/vulkan.h>

#include "VulkanBackendContext.h"

class ScreenshotManager
{
    public:
        explicit ScreenshotManager(VulkanBackendContext& backendContext)
            : backendContext(backendContext)
        {
        }

        void saveImage(VkImage image,
                       VkFormat format,
                       VkExtent2D extent,
                       VkImageLayout currentLayout,
                       const std::string& outputDirectory = {}) const;

    private:
        VulkanBackendContext& backendContext;
        static uint32_t bytesPerPixelForFormat(VkFormat format);
        static bool formatIsBgr(VkFormat format);
        static std::string allocateScreenshotPath(const std::string& outputDirectory);
};
