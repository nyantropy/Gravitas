#pragma once

#include <cstdint>
#include <string>
#include <vulkan/vulkan.h>

class ScreenshotManager
{
    public:
        void saveImage(VkImage image,
                       VkFormat format,
                       VkExtent2D extent,
                       VkImageLayout currentLayout) const;

    private:
        static uint32_t bytesPerPixelForFormat(VkFormat format);
        static bool formatIsBgr(VkFormat format);
        static std::string allocateScreenshotPath();
};
