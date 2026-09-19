#pragma once

#include <vector>
#include "GtsRealizedGeometry.h"
#include <vulkan/vulkan.h>

#include "AssetTypes.h"
#include "MeshGeometryProcessor.h"
#include "GtsStaticVertex.h"
#include "GtsGeometryMetadata.h"

struct MeshResource 
{
    std::vector<GtsStaticVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<gts::rendering::SubmeshAssetData> submeshes;
    MeshGeometryMetadata metadata;
    std::shared_ptr<const GtsRealizedGeometry> realizedGeometry;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;

    VkDeviceSize vertexCapacityBytes = 0;
    VkDeviceSize indexCapacityBytes = 0;
    bool hostVisibleProcedural = false;
};
