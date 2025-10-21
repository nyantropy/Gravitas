#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>

#include "FrameSyncObjectsConfig.h"

class FrameSyncObjects
{
public:
    explicit FrameSyncObjects(const FrameSyncObjectsConfig& config);
    ~FrameSyncObjects();

    FrameSyncObjects(const FrameSyncObjects&) = delete;
    FrameSyncObjects& operator=(const FrameSyncObjects&) = delete;

    FrameSyncObjects(FrameSyncObjects&&) noexcept = default;
    FrameSyncObjects& operator=(FrameSyncObjects&&) noexcept = default;

    VkSemaphore getImageAvailableSemaphore(size_t frameIndex) const { return imageAvailableSemaphores[frameIndex]; }
    VkSemaphore getRenderFinishedSemaphore(size_t frameIndex) const { return renderFinishedSemaphores[frameIndex]; }
    VkFence getInFlightFence(size_t frameIndex) const { return inFlightFences[frameIndex]; }

    uint32_t getFrameCount() const { return static_cast<uint32_t>(imageAvailableSemaphores.size()); }

private:
    void createSyncObjects();

    FrameSyncObjectsConfig config;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
};