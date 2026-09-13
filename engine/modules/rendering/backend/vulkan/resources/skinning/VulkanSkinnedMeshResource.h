#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "assets/processing/geometry/skinned/GtsPreparedSkinnedMesh.h"

class VulkanSkinningBuffer;
class VulkanSkinPaletteBuffer;

// Explicit skinned counterpart to the existing static-only MeshResource.
class VulkanSkinnedMeshResource
{
    public:
    VulkanSkinnedMeshResource(VkDevice device, VkPhysicalDevice physicalDevice, const GtsPreparedSkinnedMesh& mesh);
    ~VulkanSkinnedMeshResource();
    VulkanSkinnedMeshResource(const VulkanSkinnedMeshResource&)            = delete;
    VulkanSkinnedMeshResource& operator=(const VulkanSkinnedMeshResource&) = delete;

    static void                                     validate(const GtsPreparedSkinnedMesh& mesh);
    const std::vector<GtsPreparedSkinnedPrimitive>& primitives() const
    {
        return ranges;
    }
    uint32_t requiredPaletteSize() const
    {
        return paletteSize;
    }

    // Caller binds the skinned pipeline, scene sets 0..3, material constants and dynamic viewport/scissor.
    // instanceObjects uses the existing uint32 object-SSBO-slot stream at binding 1.
    // Resources must outlive submitted draws; geometry stays in stored/bind space.
    void drawPrimitive(VkCommandBuffer                commandBuffer,
                       VkPipelineLayout               layout,
                       const VulkanSkinPaletteBuffer& palette,
                       uint32_t                       frame,
                       uint32_t                       primitive,
                       VkBuffer                       instanceObjects,
                       uint32_t                       instanceCount = 1,
                       uint32_t                       firstInstance = 0) const;

    private:
    std::unique_ptr<VulkanSkinningBuffer>    vertices;
    std::unique_ptr<VulkanSkinningBuffer>    indices;
    std::vector<GtsPreparedSkinnedPrimitive> ranges;
    uint32_t                                 paletteSize = 0;
};
