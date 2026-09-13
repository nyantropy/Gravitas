#include "VulkanSkinningBuffer.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include "MemoryUtil.hpp"

VulkanSkinningBuffer::VulkanSkinningBuffer(VkDevice           device,
                                           VkPhysicalDevice   physicalDevice,
                                           VkDeviceSize       bytes,
                                           VkBufferUsageFlags usage)
    : device(device), bytes(bytes)
{
    if (!device || !physicalDevice || bytes == 0 || bytes > std::numeric_limits<size_t>::max())
        throw std::invalid_argument("Invalid skinned buffer allocation");
    try
    {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size        = bytes;
        info.usage       = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &info, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("Cannot create skinned buffer");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex =
            MemoryUtil::findMemoryType(physicalDevice,
                                       requirements.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(device, &allocation, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("Cannot allocate skinned buffer memory");
        if (vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS ||
            vkMapMemory(device, memory, 0, bytes, 0, &mapped) != VK_SUCCESS)
            throw std::runtime_error("Cannot bind/map skinned buffer memory");
    }
    catch (...)
    {
        release();
        throw;
    }
}

VulkanSkinningBuffer::~VulkanSkinningBuffer()
{
    release();
}

void VulkanSkinningBuffer::release()
{
    if (mapped)
        vkUnmapMemory(device, memory);
    if (buffer)
        vkDestroyBuffer(device, buffer, nullptr);
    if (memory)
        vkFreeMemory(device, memory, nullptr);
}

void VulkanSkinningBuffer::write(const void* data, VkDeviceSize size)
{
    if (!data || size > bytes)
        throw std::invalid_argument("Invalid skinned buffer write");
    std::memcpy(mapped, data, static_cast<size_t>(size));
}
