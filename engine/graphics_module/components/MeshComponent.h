#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "Vertex.h"

struct MeshComponent 
{
    std::vector<Vertex> vertices;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;

    std::vector<uint32_t> indices;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
};