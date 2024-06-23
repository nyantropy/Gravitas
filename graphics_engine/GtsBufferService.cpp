#include "GtsBufferService.hpp"

void GtsBufferService::createVertexBuffer(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice,
 std::vector<Vertex>& vertices, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory) 
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    GtsBufferService::createBuffer(vlogicaldevice, vphysicaldevice, 
    bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(vlogicaldevice->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t) bufferSize);
    vkUnmapMemory(vlogicaldevice->getDevice(), stagingBufferMemory);

    GtsBufferService::createBuffer(vlogicaldevice, vphysicaldevice,
    bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    GtsBufferService::copyBuffer(vlogicaldevice, stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(vlogicaldevice->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(vlogicaldevice->getDevice(), stagingBufferMemory, nullptr);
}

void GtsBufferService::createBuffer(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice,
 VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) 
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(vlogicaldevice->getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vlogicaldevice->getDevice(), buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vphysicaldevice->findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(vlogicaldevice->getDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(vlogicaldevice->getDevice(), buffer, bufferMemory, 0);
}

void GtsBufferService::copyBuffer(VulkanLogicalDevice* vlogicaldevice, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) 
{
    VkCommandBuffer commandBuffer = GtsBufferService::beginSingleTimeCommands(vlogicaldevice);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(vlogicaldevice, commandBuffer);
}

VkCommandBuffer GtsBufferService::beginSingleTimeCommands(VulkanLogicalDevice* vlogicaldevice) 
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = vlogicaldevice->getCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(vlogicaldevice->getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void GtsBufferService::endSingleTimeCommands(VulkanLogicalDevice* vlogicaldevice, VkCommandBuffer commandBuffer) 
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(vlogicaldevice->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vlogicaldevice->getGraphicsQueue());

    vkFreeCommandBuffers(vlogicaldevice->getDevice(), vlogicaldevice->getCommandPool(), 1, &commandBuffer);
}

void GtsBufferService::copyBufferToImage(VulkanLogicalDevice* vlogicaldevice, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) 
{
    VkCommandBuffer commandBuffer = GtsBufferService::beginSingleTimeCommands(vlogicaldevice);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    GtsBufferService::endSingleTimeCommands(vlogicaldevice, commandBuffer);
}

void GtsBufferService::copyImageToBuffer(VulkanLogicalDevice* vlogicaldevice, VkImage image, VkBuffer buffer, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = GtsBufferService::beginSingleTimeCommands(vlogicaldevice);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

    GtsBufferService::endSingleTimeCommands(vlogicaldevice, commandBuffer);
}


void GtsBufferService::createIndexBuffer(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice, std::vector<uint32_t> indices,
                    VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory) 
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    GtsBufferService::createBuffer(vlogicaldevice, vphysicaldevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(vlogicaldevice->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t) bufferSize);
    vkUnmapMemory(vlogicaldevice->getDevice(), stagingBufferMemory);

    GtsBufferService::createBuffer(vlogicaldevice, vphysicaldevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    GtsBufferService::copyBuffer(vlogicaldevice, stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(vlogicaldevice->getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(vlogicaldevice->getDevice(), stagingBufferMemory, nullptr);
}

void GtsBufferService::createUniformBuffers(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice, std::vector<VkBuffer>& uniformBuffers, std::vector<VkDeviceMemory>& uniformBuffersMemory,
 std::vector<void*>& uniformBuffersMapped, int frames_in_flight) 
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(frames_in_flight);
    uniformBuffersMemory.resize(frames_in_flight);
    uniformBuffersMapped.resize(frames_in_flight);

    for (size_t i = 0; i < frames_in_flight; i++) 
    {
        GtsBufferService::createBuffer(vlogicaldevice, vphysicaldevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

        vkMapMemory(vlogicaldevice->getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}