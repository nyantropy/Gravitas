#pragma once

#include <vulkan/vulkan.h>

// Backend-private mapped allocation shared by skinned geometry and palette resources.
class VulkanSkinningBuffer
{
    public:
    VulkanSkinningBuffer(VkDevice           device,
                         VkPhysicalDevice   physicalDevice,
                         VkDeviceSize       bytes,
                         VkBufferUsageFlags usage);
    ~VulkanSkinningBuffer();
    VulkanSkinningBuffer(const VulkanSkinningBuffer&)            = delete;
    VulkanSkinningBuffer& operator=(const VulkanSkinningBuffer&) = delete;

    VkBuffer handle() const
    {
        return buffer;
    }
    VkDeviceSize capacity() const
    {
        return bytes;
    }
    void write(const void* data, VkDeviceSize size);

    private:
    void           release();
    VkDevice       device;
    VkDeviceSize   bytes;
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void*          mapped = nullptr;
};
