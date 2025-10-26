#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>

#include "vcsheet.h"
#include "GraphicsConstants.h"

// manages VkSemaphore and VkFence objects to ensure a clean rendering process
class FrameSyncObjects
{
    public:
        FrameSyncObjects();
        ~FrameSyncObjects();

        VkSemaphore getImageAvailableSemaphore(size_t frameIndex) const { return imageAvailableSemaphores[frameIndex]; }
        VkSemaphore getRenderFinishedSemaphore(size_t imageIndex) const { return renderFinishedSemaphores[imageIndex]; }
        VkFence getInFlightFence(size_t frameIndex) const { return inFlightFences[frameIndex]; }
        uint32_t getFrameCount() const { return static_cast<uint32_t>(imageAvailableSemaphores.size()); }

    private:
        void createSyncObjects();

        std::vector<VkSemaphore> imageAvailableSemaphores;   // per-frame
        std::vector<VkSemaphore> renderFinishedSemaphores;   // per-swapchain-image
        std::vector<VkFence> inFlightFences;                // per-frame
};
