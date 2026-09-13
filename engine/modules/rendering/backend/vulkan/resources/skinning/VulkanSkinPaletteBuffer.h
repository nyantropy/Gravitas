#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

struct GtsSkinPalette;
class VulkanSkinningBuffer;

class VulkanSkinPaletteBuffer
{
    public:
    static constexpr uint32_t DescriptorSet     = 4;
    static constexpr uint32_t DescriptorBinding = 0;
    static constexpr uint32_t MatrixStride      = 64;

    VulkanSkinPaletteBuffer(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t frameCount);
    ~VulkanSkinPaletteBuffer();
    VulkanSkinPaletteBuffer(const VulkanSkinPaletteBuffer&)            = delete;
    VulkanSkinPaletteBuffer& operator=(const VulkanSkinPaletteBuffer&) = delete;

    static VkDescriptorSetLayoutBinding layoutBinding();
    VkDescriptorSetLayout               layout() const
    {
        return descriptorLayout;
    }
    uint32_t        matrixCount(uint32_t frame) const;
    VkDescriptorSet descriptorSet(uint32_t frame) const;

    // Update only after this frame slot's fence has completed and before recording/submitting its draws.
    // Destroy only after all uses complete. Device and physicalDevice outlive this resource.
    void update(uint32_t frame, const GtsSkinPalette& palette);

    private:
    VkDevice                                           device;
    VkPhysicalDevice                                   physicalDevice;
    VkDeviceSize                                       maxBytes;
    VkDescriptorSetLayout                              descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool                                   pool             = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet>                       sets;
    std::vector<std::unique_ptr<VulkanSkinningBuffer>> buffers;
    std::vector<uint32_t>                              counts;
};
