#include "GtsFrameGraph.h"

#include <queue>
#include <set>
#include <unordered_map>

#include "vcsheet.h"
#include "ImageUtil.hpp"
#include "MemoryUtil.hpp"

// ── Resource registration ─────────────────────────────────────────────────

GtsResourceHandle GtsFrameGraph::importResource(
    const std::string& name,
    VkImage            image,
    VkImageView        imageView,
    VkFormat           format,
    VkExtent2D         extent,
    VkImageUsageFlags  usage,
    VkImageAspectFlags aspectMask,
    VkImageLayout      initialLayout)
{
    GtsFrameGraphResource res{};
    res.name          = name;
    res.type          = GtsResourceType::Imported;
    res.format        = format;
    res.extent        = extent;
    res.usage         = usage;
    res.aspectMask    = aspectMask;
    res.currentLayout = initialLayout;
    res.image         = image;
    res.imageView     = imageView;
    res.memory        = VK_NULL_HANDLE;

    GtsResourceHandle handle = static_cast<GtsResourceHandle>(resources.size());
    resources.push_back(res);
    return handle;
}

GtsResourceHandle GtsFrameGraph::createTransientResource(
    const std::string& name,
    VkFormat           format,
    VkExtent2D         extent,
    VkImageUsageFlags  usage,
    VkImageAspectFlags aspectMask)
{
    GtsFrameGraphResource res{};
    res.name          = name;
    res.type          = GtsResourceType::Transient;
    res.format        = format;
    res.extent        = extent;
    res.usage         = usage;
    res.aspectMask    = aspectMask;
    res.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    res.image         = VK_NULL_HANDLE;
    res.imageView     = VK_NULL_HANDLE;
    res.memory        = VK_NULL_HANDLE;

    GtsResourceHandle handle = static_cast<GtsResourceHandle>(resources.size());
    resources.push_back(res);
    return handle;
}

// ── Swapchain handle management ───────────────────────────────────────────

void GtsFrameGraph::registerSwapchainHandle(uint32_t imageIndex, GtsResourceHandle handle)
{
    if (swapchainHandles.size() <= imageIndex)
        swapchainHandles.resize(imageIndex + 1, GTS_INVALID_RESOURCE);
    swapchainHandles[imageIndex] = handle;
}

GtsResourceHandle GtsFrameGraph::getSwapchainHandle(uint32_t imageIndex) const
{
    if (imageIndex >= swapchainHandles.size())
        throw std::runtime_error("GtsFrameGraph: swapchain imageIndex out of range");
    return swapchainHandles[imageIndex];
}

bool GtsFrameGraph::isSwapchainHandle(GtsResourceHandle handle) const
{
    for (auto h : swapchainHandles)
        if (h == handle) return true;
    return false;
}

GtsResourceHandle GtsFrameGraph::resolveHandle(GtsResourceHandle handle, uint32_t imageIndex) const
{
    if (isSwapchainHandle(handle))
        return swapchainHandles[imageIndex];
    return handle;
}

// ── Stage registration ────────────────────────────────────────────────────

void GtsFrameGraph::addStage(std::unique_ptr<GtsRenderStage> stage)
{
    stages.push_back(std::move(stage));
}

// ── Access declaration ────────────────────────────────────────────────────

void GtsFrameGraph::declareRead(GtsRenderStage*      stage,
                                 GtsResourceHandle    handle,
                                 VkPipelineStageFlags stageMask,
                                 VkAccessFlags        accessMask,
                                 VkImageLayout        expectedLayout)
{
    StageAccess acc{};
    acc.stage    = stage;
    acc.resource = handle;
    acc.access   = { stageMask, accessMask, expectedLayout };
    acc.isWrite  = false;
    accesses.push_back(acc);
}

void GtsFrameGraph::declareWrite(GtsRenderStage*      stage,
                                  GtsResourceHandle    handle,
                                  VkPipelineStageFlags stageMask,
                                  VkAccessFlags        accessMask,
                                  VkImageLayout        requiredLayout)
{
    StageAccess acc{};
    acc.stage    = stage;
    acc.resource = handle;
    acc.access   = { stageMask, accessMask, requiredLayout };
    acc.isWrite  = true;
    accesses.push_back(acc);
}

// ── Resource accessors ────────────────────────────────────────────────────

const GtsFrameGraphResource& GtsFrameGraph::getResource(GtsResourceHandle handle) const
{
    return resources[handle];
}

GtsFrameGraphResource& GtsFrameGraph::getResource(GtsResourceHandle handle)
{
    return resources[handle];
}

// ── Compile ───────────────────────────────────────────────────────────────

void GtsFrameGraph::compile()
{
    // 1. Call declareResources on all stages.
    for (auto& stage : stages)
        stage->declareResources(*this);

    uint32_t n = static_cast<uint32_t>(stages.size());

    // 2. Build dependency graph (Kahn's algorithm).
    std::unordered_map<GtsRenderStage*, uint32_t> stageIdx;
    for (uint32_t i = 0; i < n; ++i)
        stageIdx[stages[i].get()] = i;

    // Group accesses by resource.
    std::unordered_map<GtsResourceHandle, std::vector<uint32_t>> writersByResource;
    std::unordered_map<GtsResourceHandle, std::vector<uint32_t>> readersByResource;

    for (const auto& acc : accesses)
    {
        uint32_t si = stageIdx[acc.stage];
        if (acc.isWrite)
            writersByResource[acc.resource].push_back(si);
        else
            readersByResource[acc.resource].push_back(si);
    }

    // Build adjacency: writer → reader dependency.
    std::vector<std::vector<uint32_t>> adj(n);
    std::vector<uint32_t> inDegree(n, 0);
    std::set<std::pair<uint32_t, uint32_t>> edgeSet;

    for (auto& [res, writers] : writersByResource)
    {
        // writer → readers
        auto rIt = readersByResource.find(res);
        if (rIt != readersByResource.end())
        {
            for (uint32_t w : writers)
                for (uint32_t r : rIt->second)
                    if (w != r && edgeSet.insert({w, r}).second)
                    { adj[w].push_back(r); inDegree[r]++; }
        }

        // Serialize multiple writers to the same resource in registration order.
        for (size_t i = 0; i + 1 < writers.size(); ++i)
        {
            uint32_t a = writers[i], b = writers[i + 1];
            if (edgeSet.insert({a, b}).second)
            { adj[a].push_back(b); inDegree[b]++; }
        }
    }

    // BFS topological sort.
    std::queue<uint32_t> q;
    for (uint32_t i = 0; i < n; ++i)
        if (inDegree[i] == 0) q.push(i);

    compiledOrder.clear();
    while (!q.empty())
    {
        uint32_t cur = q.front(); q.pop();
        compiledOrder.push_back(cur);
        for (uint32_t next : adj[cur])
            if (--inDegree[next] == 0) q.push(next);
    }

    if (compiledOrder.size() != n)
        throw std::runtime_error("GtsFrameGraph: cycle detected in render stage dependencies");

    // 3. Pre-compute barrier points between stages.
    //    Track layout per resource using registration-order initial layouts.
    std::unordered_map<GtsResourceHandle, VkImageLayout> trackedLayouts;
    for (uint32_t hi = 0; hi < static_cast<uint32_t>(resources.size()); ++hi)
        trackedLayouts[hi] = resources[hi].currentLayout;

    for (uint32_t orderIdx = 0; orderIdx < static_cast<uint32_t>(compiledOrder.size()); ++orderIdx)
    {
        uint32_t si = compiledOrder[orderIdx];
        GtsRenderStage* stage = stages[si].get();

        for (const auto& acc : accesses)
        {
            if (acc.stage != stage) continue;

            VkImageLayout currentLayout = trackedLayouts[acc.resource];

            // Insert a barrier only if there is an explicit layout change needed
            // (not from UNDEFINED — render passes handle their own initial transitions).
            if (acc.access.layout != currentLayout &&
                currentLayout != VK_IMAGE_LAYOUT_UNDEFINED)
            {
                // Determine source sync from aspect type.
                VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkAccessFlags        srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                if (resources[acc.resource].aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
                {
                    srcStage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }

                BarrierPoint bp{};
                bp.handle        = acc.resource;
                bp.oldLayout     = currentLayout;
                bp.newLayout     = acc.access.layout;
                bp.srcStageMask  = srcStage;
                bp.srcAccessMask = srcAccess;
                bp.dstStageMask  = acc.access.stageMask;
                bp.dstAccessMask = acc.access.accessMask;
                bp.stageIndex    = orderIdx;
                barriers.push_back(bp);
            }

            if (acc.isWrite)
                trackedLayouts[acc.resource] = acc.access.layout;
        }
    }

    // 4. Allocate transient resources.
    for (auto& res : resources)
        if (res.type == GtsResourceType::Transient)
            allocateTransientResource(res);

    compiled = true;
}

// ── Execute ───────────────────────────────────────────────────────────────

void GtsFrameGraph::execute(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t currentFrame)
{
    // Reset swapchain resource layouts — they start UNDEFINED each frame from
    // the frame graph's perspective (the render pass initialLayout handles the
    // actual transition from whatever the GPU left the image in).
    for (auto h : swapchainHandles)
        resources[h].currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    for (uint32_t orderIdx = 0; orderIdx < static_cast<uint32_t>(compiledOrder.size()); ++orderIdx)
    {
        uint32_t si = compiledOrder[orderIdx];

        // Insert pre-computed barriers for this stage.
        for (const auto& bp : barriers)
        {
            if (bp.stageIndex != orderIdx) continue;

            GtsResourceHandle resolvedHandle = resolveHandle(bp.handle, imageIndex);
            GtsFrameGraphResource& res = resources[resolvedHandle];

            if (res.currentLayout != bp.newLayout)
            {
                insertBarrierIfNeeded(cmd, res,
                    bp.srcStageMask, bp.srcAccessMask,
                    bp.dstStageMask, bp.dstAccessMask,
                    bp.newLayout);
            }
        }

        stages[si]->record(cmd, *this, imageIndex, currentFrame);

        // Update layout tracking for all write accesses of this stage.
        for (const auto& acc : accesses)
        {
            if (acc.stage != stages[si].get() || !acc.isWrite) continue;
            GtsResourceHandle resolvedHandle = resolveHandle(acc.resource, imageIndex);
            resources[resolvedHandle].currentLayout = acc.access.layout;
        }
    }

    dataBlackboard.clear();
}

// ── Barrier insertion ─────────────────────────────────────────────────────

void GtsFrameGraph::insertBarrierIfNeeded(VkCommandBuffer        cmd,
                                           GtsFrameGraphResource& resource,
                                           VkPipelineStageFlags   srcStageMask,
                                           VkAccessFlags          srcAccessMask,
                                           VkPipelineStageFlags   dstStageMask,
                                           VkAccessFlags          dstAccessMask,
                                           VkImageLayout          newLayout)
{
    if (resource.currentLayout == newLayout && srcAccessMask == 0)
        return;

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = resource.currentLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = resource.image;
    barrier.subresourceRange.aspectMask     = resource.aspectMask;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.srcAccessMask                   = srcAccessMask;
    barrier.dstAccessMask                   = dstAccessMask;

    vkCmdPipelineBarrier(cmd,
        srcStageMask, dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    resource.currentLayout = newLayout;
}

// ── Transient resource allocation ─────────────────────────────────────────

void GtsFrameGraph::allocateTransientResource(GtsFrameGraphResource& res)
{
    VkDevice dev = vcsheet::getDevice();

    ImageUtil::createImage(dev,
        res.extent.width, res.extent.height,
        res.format, VK_IMAGE_TILING_OPTIMAL,
        res.usage, res.image);

    MemoryUtil::allocateImageMemory(dev, vcsheet::getPhysicalDevice(),
        res.image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, res.memory);

    res.imageView = ImageUtil::createImageView(dev, res.image, res.format, res.aspectMask);
}

// ── Cleanup ───────────────────────────────────────────────────────────────

void GtsFrameGraph::freeTransientResources()
{
    VkDevice dev = vcsheet::getDevice();
    for (auto& res : resources)
    {
        if (res.type != GtsResourceType::Transient) continue;
        if (res.imageView != VK_NULL_HANDLE)
        { vkDestroyImageView(dev, res.imageView, nullptr); res.imageView = VK_NULL_HANDLE; }
        if (res.image != VK_NULL_HANDLE)
        { vkDestroyImage(dev, res.image, nullptr);         res.image     = VK_NULL_HANDLE; }
        if (res.memory != VK_NULL_HANDLE)
        { vkFreeMemory(dev, res.memory, nullptr);          res.memory    = VK_NULL_HANDLE; }
    }
}
