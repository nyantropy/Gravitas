#pragma once

#include <vulkan/vulkan.h>

#include "BufferUtil.hpp"
#include "vcsheet.h"

// Host-visible, persistently-mapped GPU buffer that grows on demand.
// Owns its VkBuffer, VkDeviceMemory, and mapped pointer.
// Capacity is tracked in bytes. Call ensureCapacity before writing each frame.
// Use for vertex and index buffers that are updated every frame.
class VulkanDynamicBuffer
{
public:
    explicit VulkanDynamicBuffer(VkBufferUsageFlags usage, VkDeviceSize initialBytes)
        : usage(usage)
    {
        allocate(initialBytes);
    }

    ~VulkanDynamicBuffer()
    {
        if (buffer != VK_NULL_HANDLE)
        {
            vkUnmapMemory(vcsheet::getDevice(), memory);
            vkDestroyBuffer(vcsheet::getDevice(), buffer, nullptr);
            vkFreeMemory(vcsheet::getDevice(), memory, nullptr);
        }
    }

    // Ensure the buffer can hold at least neededBytes. Reallocates (doubling) if needed.
    void ensureCapacity(VkDeviceSize neededBytes)
    {
        if (neededBytes <= capacityBytes) return;

        vkUnmapMemory(vcsheet::getDevice(), memory);
        vkDestroyBuffer(vcsheet::getDevice(), buffer, nullptr);
        vkFreeMemory(vcsheet::getDevice(), memory, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        mapped = nullptr;

        allocate(neededBytes * 2);
    }

    void*        getMapped()   const { return mapped; }
    VkBuffer     getBuffer()   const { return buffer; }
    VkDeviceSize getCapacity() const { return capacityBytes; }

    VulkanDynamicBuffer(const VulkanDynamicBuffer&)            = delete;
    VulkanDynamicBuffer& operator=(const VulkanDynamicBuffer&) = delete;

private:
    VkBufferUsageFlags usage;
    VkBuffer           buffer        = VK_NULL_HANDLE;
    VkDeviceMemory     memory        = VK_NULL_HANDLE;
    void*              mapped        = nullptr;
    VkDeviceSize       capacityBytes = 0;

    void allocate(VkDeviceSize bytes)
    {
        capacityBytes = bytes;

        BufferUtil::createBuffer(
            vcsheet::getDevice(),
            vcsheet::getPhysicalDevice(),
            bytes, usage,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            buffer, memory);

        vkMapMemory(vcsheet::getDevice(), memory, 0, bytes, 0, &mapped);
    }
};
