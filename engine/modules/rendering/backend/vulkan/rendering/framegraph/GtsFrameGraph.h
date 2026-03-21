#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <stdexcept>

#include "GtsFrameGraphResource.h"
#include "GtsRenderStage.h"

class GtsFrameGraph
{
public:
    // ── Resource registration (call before compile) ───────────────────────

    // Import a persistent external resource.
    GtsResourceHandle importResource(const std::string& name,
                                     VkImage             image,
                                     VkImageView         imageView,
                                     VkFormat            format,
                                     VkExtent2D          extent,
                                     VkImageUsageFlags   usage,
                                     VkImageAspectFlags  aspectMask,
                                     VkImageLayout       initialLayout);

    // Declare a transient resource (temporary render target).
    // The graph allocates and frees its Vulkan memory.
    GtsResourceHandle createTransientResource(const std::string& name,
                                               VkFormat           format,
                                               VkExtent2D         extent,
                                               VkImageUsageFlags  usage,
                                               VkImageAspectFlags aspectMask);

    // ── Swapchain handle management ───────────────────────────────────────

    // Register a per-image swapchain handle so the graph can resolve the
    // correct VkImage for the current frame's imageIndex.
    void              registerSwapchainHandle(uint32_t imageIndex, GtsResourceHandle handle);
    GtsResourceHandle getSwapchainHandle(uint32_t imageIndex) const;

    // ── Stage registration (call before compile) ──────────────────────────

    void addStage(std::unique_ptr<GtsRenderStage> stage);

    // ── Resource access declaration (called from GtsRenderStage::declareResources) ──

    void declareRead(GtsRenderStage*      stage,
                     GtsResourceHandle    handle,
                     VkPipelineStageFlags stageMask,
                     VkAccessFlags        accessMask,
                     VkImageLayout        expectedLayout);

    void declareWrite(GtsRenderStage*      stage,
                      GtsResourceHandle    handle,
                      VkPipelineStageFlags stageMask,
                      VkAccessFlags        accessMask,
                      VkImageLayout        requiredLayout);

    // ── Data blackboard (call before execute each frame) ──────────────────

    // Register a data pointer for the current frame.
    template<typename T>
    void provideData(const T* data)
    {
        dataBlackboard[std::type_index(typeid(T))] = static_cast<const void*>(data);
    }

    // Retrieve previously provided data. Throws if not provided.
    template<typename T>
    const T& getData() const
    {
        auto it = dataBlackboard.find(std::type_index(typeid(T)));
        if (it == dataBlackboard.end())
            throw std::runtime_error(
                std::string("GtsFrameGraph: data not provided for type: ")
                + typeid(T).name());
        return *static_cast<const T*>(it->second);
    }

    // Declare that a stage depends on data of type T (informational).
    template<typename T>
    void requestData(GtsRenderStage* stage) { (void)stage; }

    // ── Resource accessors (call from GtsRenderStage::record) ────────────

    const GtsFrameGraphResource& getResource(GtsResourceHandle handle) const;
    GtsFrameGraphResource&       getResource(GtsResourceHandle handle);

    // ── Compile ───────────────────────────────────────────────────────────

    // Must be called once after all stages and resources are registered.
    // Calls declareResources() on every stage, topologically sorts by
    // resource dependency, determines barrier insertion points, and
    // allocates transient resource memory.
    void compile();

    // ── Execute ───────────────────────────────────────────────────────────

    // Called every frame. Records all stages into cmd in compiled order,
    // inserting pipeline barriers between stages as needed.
    void execute(VkCommandBuffer cmd, uint32_t imageIndex, uint32_t currentFrame);

    // ── Cleanup ───────────────────────────────────────────────────────────

    void freeTransientResources();

private:
    // ── Internal types ────────────────────────────────────────────────────

    struct StageAccess
    {
        GtsRenderStage*   stage;
        GtsResourceHandle resource;
        GtsResourceAccess access;
        bool              isWrite;
    };

    struct BarrierPoint
    {
        GtsResourceHandle    handle;
        VkImageLayout        oldLayout;
        VkImageLayout        newLayout;
        VkPipelineStageFlags srcStageMask;
        VkAccessFlags        srcAccessMask;
        VkPipelineStageFlags dstStageMask;
        VkAccessFlags        dstAccessMask;
        uint32_t             stageIndex;   // inserted before compiledOrder[stageIndex]
    };

    // ── Data ──────────────────────────────────────────────────────────────

    std::vector<GtsFrameGraphResource>           resources;
    std::vector<GtsResourceHandle>               swapchainHandles;
    std::vector<std::unique_ptr<GtsRenderStage>> stages;
    std::vector<StageAccess>                     accesses;
    std::vector<uint32_t>                        compiledOrder;
    std::vector<BarrierPoint>                    barriers;
    std::unordered_map<std::type_index, const void*> dataBlackboard;

    bool compiled = false;

    // ── Internal helpers ──────────────────────────────────────────────────

    bool isSwapchainHandle(GtsResourceHandle handle) const;

    // Remap a swapchain representative handle to the current frame's image handle.
    GtsResourceHandle resolveHandle(GtsResourceHandle handle, uint32_t imageIndex) const;

    void insertBarrierIfNeeded(VkCommandBuffer        cmd,
                                GtsFrameGraphResource& resource,
                                VkPipelineStageFlags   srcStageMask,
                                VkAccessFlags          srcAccessMask,
                                VkPipelineStageFlags   dstStageMask,
                                VkAccessFlags          dstAccessMask,
                                VkImageLayout          newLayout);

    void allocateTransientResource(GtsFrameGraphResource& res);
};
