#include "VulkanSkinPaletteBuffer.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include "VulkanSkinningBuffer.h"
#include "animation/skinning/GtsSkinPalette.h"

static_assert(sizeof(glm::mat4) == VulkanSkinPaletteBuffer::MatrixStride);
static_assert(sizeof(glm::mat4::col_type) == 16 && sizeof(float) == 4);

VkDescriptorSetLayoutBinding VulkanSkinPaletteBuffer::layoutBinding()
{
    return {DescriptorBinding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
}

VulkanSkinPaletteBuffer::VulkanSkinPaletteBuffer(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t frameCount)
    : device(device), physicalDevice(physicalDevice), sets(frameCount), buffers(frameCount), counts(frameCount)
{
    if (!device || !physicalDevice || !frameCount)
        throw std::invalid_argument("Palette requires a device and at least one frame slot");
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    maxBytes = properties.limits.maxStorageBufferRange;
    if (properties.limits.maxBoundDescriptorSets <= DescriptorSet ||
        properties.limits.maxPerStageDescriptorStorageBuffers < 2)
        throw std::runtime_error("Skinned scene requires five descriptor sets and two vertex storage buffers");
    try
    {
        const auto                      binding = layoutBinding();
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &binding;
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorLayout) != VK_SUCCESS)
            throw std::runtime_error("Cannot create skin palette descriptor layout");
        VkDescriptorPoolSize       size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets       = frameCount;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &size;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
            throw std::runtime_error("Cannot create skin palette descriptor pool");
        std::vector<VkDescriptorSetLayout> layouts(frameCount, descriptorLayout);
        VkDescriptorSetAllocateInfo        allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool     = pool;
        allocation.descriptorSetCount = frameCount;
        allocation.pSetLayouts        = layouts.data();
        if (vkAllocateDescriptorSets(device, &allocation, sets.data()) != VK_SUCCESS)
            throw std::runtime_error("Cannot allocate skin palette descriptors");
    }
    catch (...)
    {
        if (pool)
            vkDestroyDescriptorPool(device, pool, nullptr);
        if (descriptorLayout)
            vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
        throw;
    }
}

VulkanSkinPaletteBuffer::~VulkanSkinPaletteBuffer()
{
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
}

uint32_t VulkanSkinPaletteBuffer::matrixCount(uint32_t frame) const
{
    return counts.at(frame);
}

VkDescriptorSet VulkanSkinPaletteBuffer::descriptorSet(uint32_t frame) const
{
    if (!counts.at(frame))
        throw std::logic_error("Skin palette frame has not been uploaded");
    return sets.at(frame);
}

void VulkanSkinPaletteBuffer::update(uint32_t frame, const GtsSkinPalette& palette)
{
    auto&      buffer = buffers.at(frame);
    const auto count  = palette.matrices.size();
    if (!count || count > maxBytes / MatrixStride || count > std::numeric_limits<uint32_t>::max())
        throw std::invalid_argument("Skin palette is empty or exceeds device storage range");
    for (size_t slot = 0; slot < count; ++slot)
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (!std::isfinite(palette.matrices[slot][c][r]))
                    throw std::invalid_argument("Non-finite skin palette matrix at slot " + std::to_string(slot));
    const VkDeviceSize bytes = count * MatrixStride;
    if (!buffer || buffer->capacity() < bytes)
        buffer =
            std::make_unique<VulkanSkinningBuffer>(device, physicalDevice, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    buffer->write(palette.matrices.data(), bytes);
    VkDescriptorBufferInfo info{buffer->handle(), 0, bytes};
    VkWriteDescriptorSet   write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet          = sets.at(frame);
    write.dstBinding      = DescriptorBinding;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo     = &info;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    counts.at(frame) = static_cast<uint32_t>(count);
}
