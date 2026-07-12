#pragma once

#include <cstdint>
#include <future>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "GtsFrameStats.h"
#include "VulkanBackendContext.h"

class ScreenshotManager
{
    public:
        explicit ScreenshotManager(VulkanBackendContext& backendContext)
            : backendContext(backendContext)
        {
        }
        ~ScreenshotManager();

        void pollCompletedCaptures(GtsFrameStats& stats);

        bool scheduleCapture(VkCommandBuffer commandBuffer,
                             VkImage image,
                             VkFormat format,
                             VkExtent2D extent,
                             VkImageLayout currentLayout,
                             VkFence completionFence,
                             const std::string& outputDirectory,
                             GtsFrameStats& stats);

        void waitForPendingWork();
        void cancelCapturesForFence(VkFence completionFence);

    private:
        struct CaptureSlot
        {
            VkBuffer stagingBuffer = VK_NULL_HANDLE;
            VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
            VkDeviceSize capacity = 0;
            VkDeviceSize byteSize = 0;
            VkFormat format = VK_FORMAT_UNDEFINED;
            VkExtent2D extent{0, 0};
            uint32_t bytesPerPixel = 0;
            bool bgrSource = false;
            bool pendingGpu = false;
            VkFence completionFence = VK_NULL_HANDLE;
            std::string outputPath;
        };

        static constexpr size_t MaxPendingCaptures = 3;

        VulkanBackendContext& backendContext;
        std::vector<CaptureSlot> captureSlots;
        std::vector<std::future<void>> pendingWrites;
        bool pendingCaptureWarningLogged = false;

        static uint32_t bytesPerPixelForFormat(VkFormat format);
        static bool formatIsBgr(VkFormat format);
        static std::string allocateScreenshotPath(const std::string& outputDirectory);
        static void recordImageCopy(VkCommandBuffer commandBuffer,
                                    VkImage image,
                                    VkBuffer stagingBuffer,
                                    VkExtent2D extent,
                                    VkImageLayout currentLayout);
        void pruneCompletedWrites();
        void waitForPendingWrites();
        void updatePendingStats(GtsFrameStats* stats) const;
        CaptureSlot* acquireCaptureSlot();
        void ensureSlotCapacity(CaptureSlot& slot, VkDeviceSize requiredSize);
        void completeCapture(CaptureSlot& slot, GtsFrameStats* stats);
        void destroyCaptureSlot(CaptureSlot& slot);
        void destroyCaptureSlots();
};
