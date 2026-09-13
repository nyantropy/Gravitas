#pragma once

#include <array>
#include <map>
#include <memory>
#include "core/skinning/SkinnedFrameData.h"
#include "VulkanSkinnedMeshResource.h"
#include "VulkanSkinPaletteBuffer.h"
#include "VulkanSkinningBuffer.h"
#include "VulkanPipeline.hpp"
#include "RenderResourceManager.hpp"
#include "GraphicsConstants.h"

// Called from scene recording after the current frame fence and command-buffer reset.
// Frame references retain GPU resources until that frame can safely be reused.
class VulkanSkinnedSceneRenderer
{
public:
    VulkanSkinnedSceneRenderer(VulkanBackendContext&, DescriptorSetManager&, RenderResourceManager&, VkRenderPass);
    void prepare(const SkinnedFrameData&, uint32_t frame);
    void record(VkCommandBuffer, uint32_t frame);
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
    bool hasDraws(uint32_t frame) const { return !frames.at(frame).empty(); }
private:
    struct Geometry
    {
        std::shared_ptr<const GtsSkinnedModelData> source;
        std::vector<std::unique_ptr<VulkanSkinnedMeshResource>> parts;
        std::vector<MaterialFrameState> materials;
    };
    struct Instance
    {
        std::weak_ptr<const GtsSkinnedModelInstance> source;
        std::shared_ptr<Geometry> geometry;
        std::vector<std::unique_ptr<VulkanSkinPaletteBuffer>> palettes;
        std::unique_ptr<VulkanSkinningBuffer> objectIndex;
        RenderResourceManager* resources = nullptr;
        ssbo_id_type objectSlot = 0;
        bool ownsSlot = false;
        ~Instance() { if (ownsSlot) resources->releaseObjectSlot(objectSlot); }
    };
    VulkanPipeline& pipeline(const MaterialFrameState&, VkDescriptorSetLayout);
    VulkanBackendContext& context;
    DescriptorSetManager& descriptors;
    RenderResourceManager& resources;
    VkRenderPass renderPass;
    view_id_type camera = 0;
    std::map<const GtsSkinnedModelData*, std::weak_ptr<Geometry>> geometries;
    std::map<const GtsSkinnedModelInstance*, std::shared_ptr<Instance>> instances;
    std::map<std::pair<MaterialShaderFamily, bool>, std::unique_ptr<VulkanPipeline>> pipelines;
    std::array<std::vector<std::shared_ptr<Instance>>, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> frames;
};
