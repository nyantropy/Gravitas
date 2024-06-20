#pragma once

#include <vulkan/vulkan.h>

#include "VulkanPhysicalDevice.hpp"
#include "VulkanLogicalDevice.hpp"
#include "Vertex.h"

class GtsBufferService
{
    public:
        GtsBufferService() = delete;

        static void createVertexBuffer(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice,
                    std::vector<Vertex>& vertices, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory);

        static void createBuffer(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice,
                    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        static void copyBuffer(VulkanLogicalDevice* vlogicaldevice, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        static VkCommandBuffer beginSingleTimeCommands(VulkanLogicalDevice* vlogicaldevice);
        static void endSingleTimeCommands(VulkanLogicalDevice* vlogicaldevice, VkCommandBuffer commandBuffer);
        static void copyBufferToImage(VulkanLogicalDevice* vlogicaldevice, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
};