#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "VulkanBackendContext.h"

struct VulkanGpuTimestampResult
{
    bool available = false;
    bool supported = false;
    float frameMs = 0.0f;
    float sceneMs = 0.0f;
    float particleMs = 0.0f;
    float uiMs = 0.0f;
    std::string status;
};

class VulkanTimestampManager
{
public:
    static constexpr uint32_t MaxQueriesPerFrame = 64;

    VulkanTimestampManager(VulkanBackendContext& backendContext,
                           uint32_t framesInFlight,
                           bool enabled);
    ~VulkanTimestampManager();

    bool isEnabled() const
    {
        return enabled && supported;
    }

    bool isSupported() const
    {
        return supported;
    }

    const std::string& status() const
    {
        return statusMessage;
    }

    VulkanGpuTimestampResult collectFrame(uint32_t frameIndex);

    void beginFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void beginStage(VkCommandBuffer commandBuffer,
                    uint32_t frameIndex,
                    const std::string& stageName);
    void endStage(VkCommandBuffer commandBuffer,
                  uint32_t frameIndex,
                  const std::string& stageName);
    void endFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex);

private:
    struct StageSample
    {
        std::string name;
        uint32_t beginQuery = 0;
        uint32_t endQuery = 0;
    };

    struct PendingStage
    {
        std::string name;
        uint32_t beginQuery = 0;
        bool valid = false;
    };

    struct FrameQueries
    {
        VkQueryPool queryPool = VK_NULL_HANDLE;
        uint32_t nextQuery = 0;
        uint32_t queryCount = 0;
        uint32_t frameBeginQuery = 0;
        uint32_t frameEndQuery = 0;
        bool recording = false;
        bool pending = false;
        std::vector<StageSample> samples;
        std::vector<PendingStage> pendingStages;
    };

    VulkanBackendContext& backendContext;
    bool requested = false;
    bool enabled = false;
    bool supported = false;
    bool warningLogged = false;
    float timestampPeriodNs = 0.0f;
    uint32_t timestampValidBits = 0;
    uint64_t timestampMask = ~0ull;
    std::string statusMessage = "disabled";
    std::vector<FrameQueries> frames;

    void detectCapabilities();
    void createQueryPools(uint32_t framesInFlight);
    void destroyQueryPools();
    uint32_t allocateQuery(FrameQueries& frame);
    float elapsedMs(uint64_t begin, uint64_t end) const;
    bool validFrameIndex(uint32_t frameIndex) const;
    void disable(std::string status);
    void logWarningOnce();
};
