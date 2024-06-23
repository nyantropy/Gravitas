#pragma once

#include <vulkan/vulkan.h>

#include "VulkanPhysicalDevice.hpp"
#include "VulkanLogicalDevice.hpp"
#include "Vertex.h"
#include "UniformBufferObject.h"

class GtsBufferService
{
    public:
        GtsBufferService() = delete;

        static void createVertexBuffer(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice,
                    std::vector<Vertex>& vertices, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory);
        static void createIndexBuffer(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice, std::vector<uint32_t> indices,
                    VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory);
        static void createUniformBuffers(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice, std::vector<VkBuffer>& uniformBuffers, std::vector<VkDeviceMemory>& uniformBuffersMemory,
                    std::vector<void*>& uniformBuffersMapped, int frames_in_flight); 

        static void createBuffer(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice,
                    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        static void copyBuffer(VulkanLogicalDevice* vlogicaldevice, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        static VkCommandBuffer beginSingleTimeCommands(VulkanLogicalDevice* vlogicaldevice);
        static void endSingleTimeCommands(VulkanLogicalDevice* vlogicaldevice, VkCommandBuffer commandBuffer);
        static void copyBufferToImage(VulkanLogicalDevice* vlogicaldevice, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        static void copyImageToBuffer(VulkanLogicalDevice* vlogicaldevice, VkImage image, VkBuffer buffer, uint32_t width, uint32_t height);
};