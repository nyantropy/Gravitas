#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "MeshGeometryProcessor.h"
#include "Vertex.h"

struct MeshResource 
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    MeshGeometryMetadata metadata;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;

    VkDeviceSize vertexCapacityBytes = 0;
    VkDeviceSize indexCapacityBytes = 0;
    bool hostVisibleProcedural = false;
};
