#pragma once

#include <cstdint>
#include <future>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "VulkanBackendContext.h"

class ScreenshotManager
{
    public:
        explicit ScreenshotManager(VulkanBackendContext& backendContext)
            : backendContext(backendContext)
        {
        }
        ~ScreenshotManager();

        void saveImage(VkImage image,
                       VkFormat format,
                       VkExtent2D extent,
                       VkImageLayout currentLayout,
                       const std::string& outputDirectory = {});

    private:
        VulkanBackendContext& backendContext;
        std::vector<std::future<void>> pendingWrites;

        static uint32_t bytesPerPixelForFormat(VkFormat format);
        static bool formatIsBgr(VkFormat format);
        static std::string allocateScreenshotPath(const std::string& outputDirectory);
        void pruneCompletedWrites();
        void waitForPendingWrites();
};
