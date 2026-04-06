#pragma once

#include <cstdint>
#include <string>
#include <vulkan/vulkan.h>

class ScreenshotManager
{
    public:
        void saveSwapchainImage(uint32_t imageIndex) const;

    private:
        static uint32_t bytesPerPixelForFormat(VkFormat format);
        static bool formatIsBgr(VkFormat format);
        static std::string allocateScreenshotPath();
};
