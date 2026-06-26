#pragma once

#include <vulkan/vulkan.h>

#include "BufferUtil.hpp"
#include "VulkanBackendContext.h"

// Host-visible, persistently-mapped GPU buffer that grows on demand.
// Owns its VkBuffer, VkDeviceMemory, and mapped pointer.
// Capacity is tracked in bytes. Call ensureCapacity before writing each frame.
// Use for vertex and index buffers that are updated every frame.
class VulkanDynamicBuffer
{
public:
    VulkanDynamicBuffer(VulkanBackendContext& backendContext,
                        VkBufferUsageFlags usage,
                        VkDeviceSize initialBytes)
        : backendContext(backendContext)
        , usage(usage)
    {
        allocate(initialBytes);
    }

    ~VulkanDynamicBuffer()
    {
        if (buffer != VK_NULL_HANDLE)
        {
            vkUnmapMemory(backendContext.device(), memory);
            vkDestroyBuffer(backendContext.device(), buffer, nullptr);
            vkFreeMemory(backendContext.device(), memory, nullptr);
        }
    }

    // Ensure the buffer can hold at least neededBytes. Reallocates (doubling) if needed.
    void ensureCapacity(VkDeviceSize neededBytes)
    {
        if (neededBytes <= capacityBytes) return;

        // Resizes are rare, but the old buffer may still be referenced by an
        // in-flight command buffer from another frame slot.
        vkDeviceWaitIdle(backendContext.device());

        vkUnmapMemory(backendContext.device(), memory);
        vkDestroyBuffer(backendContext.device(), buffer, nullptr);
        vkFreeMemory(backendContext.device(), memory, nullptr);
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
    VulkanBackendContext& backendContext;
    VkBufferUsageFlags usage;
    VkBuffer           buffer        = VK_NULL_HANDLE;
    VkDeviceMemory     memory        = VK_NULL_HANDLE;
    void*              mapped        = nullptr;
    VkDeviceSize       capacityBytes = 0;

    void allocate(VkDeviceSize bytes)
    {
        capacityBytes = bytes;

        BufferUtil::createBuffer(
            backendContext.device(),
            backendContext.physicalDevice(),
            bytes, usage,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            buffer, memory);

        vkMapMemory(backendContext.device(), memory, 0, bytes, 0, &mapped);
    }
};
