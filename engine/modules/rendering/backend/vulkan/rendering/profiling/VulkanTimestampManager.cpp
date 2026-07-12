#include "VulkanTimestampManager.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "QueueFamilyIndices.h"

VulkanTimestampManager::VulkanTimestampManager(VulkanBackendContext& backendContext,
                                               uint32_t framesInFlight,
                                               bool enabled)
    : backendContext(backendContext)
    , requested(enabled)
    , enabled(enabled)
{
    if (!requested)
    {
        statusMessage = "GPU timestamps disabled by configuration";
        return;
    }

    detectCapabilities();
    if (!supported)
    {
        logWarningOnce();
        return;
    }

    createQueryPools(framesInFlight);
}

VulkanTimestampManager::~VulkanTimestampManager()
{
    destroyQueryPools();
}

void VulkanTimestampManager::detectCapabilities()
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(backendContext.physicalDevice(), &properties);
    timestampPeriodNs = properties.limits.timestampPeriod;

    QueueFamilyIndices indices = backendContext.queueFamilyIndices();
    if (!indices.graphicsFamily.has_value())
    {
        disable("graphics queue family is unavailable");
        return;
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(backendContext.physicalDevice(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(backendContext.physicalDevice(),
                                             &queueFamilyCount,
                                             queueFamilies.data());

    const uint32_t graphicsFamily = indices.graphicsFamily.value();
    if (graphicsFamily >= queueFamilies.size())
    {
        disable("graphics queue family index is out of range");
        return;
    }

    timestampValidBits = queueFamilies[graphicsFamily].timestampValidBits;
    if (timestampPeriodNs <= 0.0f || timestampValidBits == 0)
    {
        disable("graphics queue family does not support timestamp queries");
        return;
    }

    timestampMask = timestampValidBits >= 64
        ? ~0ull
        : ((1ull << timestampValidBits) - 1ull);
    supported = true;
    statusMessage = "available";
}

void VulkanTimestampManager::createQueryPools(uint32_t framesInFlight)
{
    frames.resize(std::max(1u, framesInFlight));
    for (FrameQueries& frame : frames)
    {
        VkQueryPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        poolInfo.queryCount = MaxQueriesPerFrame;

        const VkResult result =
            vkCreateQueryPool(backendContext.device(), &poolInfo, nullptr, &frame.queryPool);
        if (result != VK_SUCCESS)
        {
            destroyQueryPools();
            disable("failed to create Vulkan timestamp query pool");
            logWarningOnce();
            return;
        }

        frame.samples.reserve(MaxQueriesPerFrame / 2u);
        frame.pendingStages.reserve(MaxQueriesPerFrame / 2u);
    }
}

void VulkanTimestampManager::destroyQueryPools()
{
    for (FrameQueries& frame : frames)
    {
        if (frame.queryPool != VK_NULL_HANDLE)
        {
            vkDestroyQueryPool(backendContext.device(), frame.queryPool, nullptr);
            frame.queryPool = VK_NULL_HANDLE;
        }
    }
    frames.clear();
}

VulkanGpuTimestampResult VulkanTimestampManager::collectFrame(uint32_t frameIndex)
{
    VulkanGpuTimestampResult result;
    result.supported = supported;
    result.status = statusMessage;
    if (!isEnabled() || !validFrameIndex(frameIndex))
        return result;

    FrameQueries& frame = frames[frameIndex];
    if (!frame.pending || frame.queryCount == 0)
        return result;

    std::vector<uint64_t> timestamps(frame.queryCount, 0);
    const VkResult queryResult =
        vkGetQueryPoolResults(backendContext.device(),
                              frame.queryPool,
                              0,
                              frame.queryCount,
                              sizeof(uint64_t) * timestamps.size(),
                              timestamps.data(),
                              sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT);
    frame.pending = false;

    if (queryResult != VK_SUCCESS)
    {
        result.status = "timestamp query results unavailable";
        return result;
    }

    result.available = true;
    result.frameMs = elapsedMs(timestamps[frame.frameBeginQuery], timestamps[frame.frameEndQuery]);

    for (const StageSample& sample : frame.samples)
    {
        const float durationMs = elapsedMs(timestamps[sample.beginQuery], timestamps[sample.endQuery]);
        if (sample.name == "SceneRenderStage")
            result.sceneMs += durationMs;
        else if (sample.name == "ParticleRenderStage")
            result.particleMs += durationMs;
        else if (sample.name == "UiRenderStage")
            result.uiMs += durationMs;
    }

    result.status = "available";
    return result;
}

void VulkanTimestampManager::beginFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
    if (!isEnabled() || !validFrameIndex(frameIndex))
        return;

    FrameQueries& frame = frames[frameIndex];
    frame.nextQuery = 0;
    frame.queryCount = 0;
    frame.samples.clear();
    frame.pendingStages.clear();
    frame.recording = true;

    vkCmdResetQueryPool(commandBuffer, frame.queryPool, 0, MaxQueriesPerFrame);
    frame.frameBeginQuery = allocateQuery(frame);
    vkCmdWriteTimestamp(commandBuffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        frame.queryPool,
                        frame.frameBeginQuery);
}

void VulkanTimestampManager::beginStage(VkCommandBuffer commandBuffer,
                                        uint32_t frameIndex,
                                        const std::string& stageName)
{
    if (!isEnabled() || !validFrameIndex(frameIndex))
        return;

    FrameQueries& frame = frames[frameIndex];
    if (!frame.recording || frame.nextQuery >= MaxQueriesPerFrame)
        return;

    PendingStage pending;
    pending.name = stageName;
    pending.beginQuery = allocateQuery(frame);
    pending.valid = true;
    vkCmdWriteTimestamp(commandBuffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        frame.queryPool,
                        pending.beginQuery);
    frame.pendingStages.push_back(std::move(pending));
}

void VulkanTimestampManager::endStage(VkCommandBuffer commandBuffer,
                                      uint32_t frameIndex,
                                      const std::string& stageName)
{
    if (!isEnabled() || !validFrameIndex(frameIndex))
        return;

    FrameQueries& frame = frames[frameIndex];
    if (!frame.recording || frame.nextQuery >= MaxQueriesPerFrame)
        return;

    auto it = std::find_if(
        frame.pendingStages.rbegin(),
        frame.pendingStages.rend(),
        [&](const PendingStage& pending)
        {
            return pending.valid && pending.name == stageName;
        });
    if (it == frame.pendingStages.rend())
        return;

    const uint32_t endQuery = allocateQuery(frame);
    vkCmdWriteTimestamp(commandBuffer,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        frame.queryPool,
                        endQuery);
    frame.samples.push_back({stageName, it->beginQuery, endQuery});
    it->valid = false;
}

void VulkanTimestampManager::endFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
    if (!isEnabled() || !validFrameIndex(frameIndex))
        return;

    FrameQueries& frame = frames[frameIndex];
    if (!frame.recording || frame.nextQuery >= MaxQueriesPerFrame)
        return;

    frame.frameEndQuery = allocateQuery(frame);
    vkCmdWriteTimestamp(commandBuffer,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        frame.queryPool,
                        frame.frameEndQuery);
    frame.queryCount = frame.nextQuery;
    frame.pending = true;
    frame.recording = false;
}

uint32_t VulkanTimestampManager::allocateQuery(FrameQueries& frame)
{
    if (frame.nextQuery >= MaxQueriesPerFrame)
        return MaxQueriesPerFrame - 1u;
    return frame.nextQuery++;
}

float VulkanTimestampManager::elapsedMs(uint64_t begin, uint64_t end) const
{
    begin &= timestampMask;
    end &= timestampMask;
    const uint64_t delta = (end - begin) & timestampMask;
    return static_cast<float>(static_cast<double>(delta) *
                              static_cast<double>(timestampPeriodNs) /
                              1000000.0);
}

bool VulkanTimestampManager::validFrameIndex(uint32_t frameIndex) const
{
    return frameIndex < frames.size() &&
           frames[frameIndex].queryPool != VK_NULL_HANDLE;
}

void VulkanTimestampManager::disable(std::string status)
{
    supported = false;
    enabled = false;
    statusMessage = std::move(status);
}

void VulkanTimestampManager::logWarningOnce()
{
    if (!requested || warningLogged)
        return;

    warningLogged = true;
    std::cout << "Vulkan GPU timestamps disabled: " << statusMessage << std::endl;
}
