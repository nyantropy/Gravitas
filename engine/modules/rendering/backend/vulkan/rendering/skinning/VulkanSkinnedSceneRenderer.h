#pragma once

#include <array>
#include <map>
#include <memory>
#include "core/model/GtsModelFrameData.h"
#include "VulkanSkinnedMeshResource.h"
#include "VulkanSkinPaletteBuffer.h"
#include "VulkanSkinningBuffer.h"
#include "VulkanPipeline.hpp"
#include "RenderResourceManager.hpp"
#include "GraphicsConstants.h"

// Per-frame references survive until the corresponding fence is waited/reset.
class VulkanSkinnedSceneRenderer
{
    public:
    VulkanSkinnedSceneRenderer(VulkanBackendContext&, DescriptorSetManager&, RenderResourceManager&, VkRenderPass);
    void   prepare(const GtsModelFrameData&, uint32_t frame);
    void   record(VkCommandBuffer, uint32_t frame);
    size_t residentGeometryCount() const
    {
        return geometries.size();
    }
    size_t residentPaletteCount() const
    {
        return palettes.size();
    }
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
    bool     hasDraws(uint32_t frame) const
    {
        return !frames.at(frame).empty();
    }

    private:
    struct Geometry
    {
        std::shared_ptr<const GtsRealizedGeometry> source;
        std::unique_ptr<VulkanSkinnedMeshResource> mesh;
    };
    struct Placement
    {
        std::weak_ptr<const void>             owner;
        std::unique_ptr<VulkanSkinningBuffer> objectIndex;
        RenderResourceManager*                resources  = nullptr;
        ssbo_id_type                          objectSlot = 0;
        bool                                  ownsSlot   = false;
        ~Placement()
        {
            if (ownsSlot)
                resources->releaseObjectSlot(objectSlot);
        }
    };
    struct Palette
    {
        std::weak_ptr<const void>                owner;
        std::unique_ptr<VulkanSkinPaletteBuffer> buffer;
    };
    struct Draw
    {
        std::shared_ptr<Geometry>  geometry;
        std::shared_ptr<Placement> placement;
        std::shared_ptr<Palette>   palette;
        MaterialFrameState         material;
        uint32_t                   primitive = 0;
    };
    using Key = std::pair<const void*, uint32_t>;
    VulkanPipeline&        pipeline(const MaterialFrameState&, VkDescriptorSetLayout);
    VulkanBackendContext&  context;
    DescriptorSetManager&  descriptors;
    RenderResourceManager& resources;
    VkRenderPass           renderPass;
    std::array<view_id_type, GraphicsConstants::MAX_FRAMES_IN_FLIGHT>                cameras{};
    std::map<const GtsRealizedGeometry*, std::shared_ptr<Geometry>>                  geometries;
    std::map<Key, std::shared_ptr<Placement>>                                        placements;
    std::map<Key, std::shared_ptr<Palette>>                                          palettes;
    std::map<std::pair<MaterialShaderFamily, bool>, std::unique_ptr<VulkanPipeline>> pipelines;
    std::array<std::vector<Draw>, GraphicsConstants::MAX_FRAMES_IN_FLIGHT>           frames;
};
