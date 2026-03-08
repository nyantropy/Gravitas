#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <memory>
#include <stdexcept>

#include "vcsheet.h"

// Creates 3 descriptor set layouts:
//   set 0 = camera UBO  (view, proj)          - VERTEX, UNIFORM_BUFFER
//   set 1 = object SSBO (array of ObjectData) - VERTEX, STORAGE_BUFFER
//   set 2 = texture sampler                   - FRAGMENT, COMBINED_IMAGE_SAMPLER
//
// Pools are fixed-size and never scale with object count.
class DescriptorSetManager
{
    public:
        explicit DescriptorSetManager(uint32_t framesInFlight) : framesInFlight(framesInFlight)
        {
            createDescriptorSetLayouts();
            createDescriptorPools();
        }

        ~DescriptorSetManager()
        {
            VkDevice device = vcsheet::getDevice();

            if (uboDescriptorPool != VK_NULL_HANDLE)
                vkDestroyDescriptorPool(device, uboDescriptorPool, nullptr);

            if (ssboDescriptorPool != VK_NULL_HANDLE)
                vkDestroyDescriptorPool(device, ssboDescriptorPool, nullptr);

            if (samplerDescriptorPool != VK_NULL_HANDLE)
                vkDestroyDescriptorPool(device, samplerDescriptorPool, nullptr);

            for (auto layout : descriptorSetLayouts)
                if (layout != VK_NULL_HANDLE)
                    vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }

        const std::array<VkDescriptorSetLayout, 3>& getDescriptorSetLayouts() const { return descriptorSetLayouts; }

        // Allocates framesInFlight descriptor sets for a camera UBO (set 0).
        std::vector<VkDescriptorSet> allocateForUniformBuffer(const std::vector<VkBuffer>& uniformBuffers, VkDeviceSize range)
        {
            std::vector<VkDescriptorSet> descriptorSets(framesInFlight);

            std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayouts[0]);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool     = uboDescriptorPool;
            allocInfo.descriptorSetCount = framesInFlight;
            allocInfo.pSetLayouts        = layouts.data();

            if (vkAllocateDescriptorSets(vcsheet::getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
                throw std::runtime_error("Failed to allocate camera UBO descriptor sets!");

            for (size_t i = 0; i < framesInFlight; i++)
            {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = uniformBuffers[i];
                bufferInfo.offset = 0;
                bufferInfo.range  = range;

                VkWriteDescriptorSet write{};
                write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet          = descriptorSets[i];
                write.dstBinding      = 0;
                write.dstArrayElement = 0;
                write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.descriptorCount = 1;
                write.pBufferInfo     = &bufferInfo;

                vkUpdateDescriptorSets(vcsheet::getDevice(), 1, &write, 0, nullptr);
            }

            return descriptorSets;
        }

        // Allocates framesInFlight descriptor sets for the single object SSBO (set 1).
        // Called once at startup; the same sets are reused for every draw every frame.
        std::vector<VkDescriptorSet> allocateForObjectSSBO(const std::vector<VkBuffer>& ssboBuffers, VkDeviceSize bufferSize)
        {
            std::vector<VkDescriptorSet> descriptorSets(framesInFlight);

            std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayouts[1]);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool     = ssboDescriptorPool;
            allocInfo.descriptorSetCount = framesInFlight;
            allocInfo.pSetLayouts        = layouts.data();

            if (vkAllocateDescriptorSets(vcsheet::getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
                throw std::runtime_error("Failed to allocate object SSBO descriptor sets!");

            for (size_t i = 0; i < framesInFlight; i++)
            {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = ssboBuffers[i];
                bufferInfo.offset = 0;
                bufferInfo.range  = bufferSize;

                VkWriteDescriptorSet write{};
                write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet          = descriptorSets[i];
                write.dstBinding      = 0;
                write.dstArrayElement = 0;
                write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.descriptorCount = 1;
                write.pBufferInfo     = &bufferInfo;

                vkUpdateDescriptorSets(vcsheet::getDevice(), 1, &write, 0, nullptr);
            }

            return descriptorSets;
        }

        std::vector<VkDescriptorSet> allocateForTexture(VkImageView imageView, VkSampler sampler)
        {
            std::vector<VkDescriptorSet> descriptorSets(framesInFlight);

            std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayouts[2]);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool     = samplerDescriptorPool;
            allocInfo.descriptorSetCount = framesInFlight;
            allocInfo.pSetLayouts        = layouts.data();

            if (vkAllocateDescriptorSets(vcsheet::getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
                throw std::runtime_error("Failed to allocate texture descriptor sets!");

            for (size_t i = 0; i < framesInFlight; i++)
            {
                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView   = imageView;
                imageInfo.sampler     = sampler;

                VkWriteDescriptorSet write{};
                write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet          = descriptorSets[i];
                write.dstBinding      = 0;
                write.dstArrayElement = 0;
                write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.descriptorCount = 1;
                write.pImageInfo      = &imageInfo;

                vkUpdateDescriptorSets(vcsheet::getDevice(), 1, &write, 0, nullptr);
            }

            return descriptorSets;
        }

    private:
        uint32_t framesInFlight;

        // [0] = camera UBO layout, [1] = object SSBO layout, [2] = sampler layout
        std::array<VkDescriptorSetLayout, 3> descriptorSetLayouts{};

        VkDescriptorPool uboDescriptorPool     = VK_NULL_HANDLE; // camera UBOs — fixed: MAX_VIEWS * framesInFlight sets
        VkDescriptorPool ssboDescriptorPool    = VK_NULL_HANDLE; // object SSBO — fixed: framesInFlight sets
        VkDescriptorPool samplerDescriptorPool = VK_NULL_HANDLE; // textures — scales with unique texture count

        void createDescriptorSetLayouts()
        {
            // set 0: camera UBO
            VkDescriptorSetLayoutBinding uboBinding{};
            uboBinding.binding            = 0;
            uboBinding.descriptorCount    = 1;
            uboBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboBinding.pImmutableSamplers = nullptr;
            uboBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

            VkDescriptorSetLayoutCreateInfo uboLayoutInfo{};
            uboLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            uboLayoutInfo.bindingCount = 1;
            uboLayoutInfo.pBindings    = &uboBinding;

            if (vkCreateDescriptorSetLayout(vcsheet::getDevice(), &uboLayoutInfo, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create camera UBO descriptor set layout!");

            // set 1: object SSBO (storage buffer — replaces the old per-object UBO)
            VkDescriptorSetLayoutBinding ssboBinding{};
            ssboBinding.binding            = 0;
            ssboBinding.descriptorCount    = 1;
            ssboBinding.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            ssboBinding.pImmutableSamplers = nullptr;
            ssboBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

            VkDescriptorSetLayoutCreateInfo ssboLayoutInfo{};
            ssboLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            ssboLayoutInfo.bindingCount = 1;
            ssboLayoutInfo.pBindings    = &ssboBinding;

            if (vkCreateDescriptorSetLayout(vcsheet::getDevice(), &ssboLayoutInfo, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create object SSBO descriptor set layout!");

            // set 2: texture sampler
            VkDescriptorSetLayoutBinding samplerBinding{};
            samplerBinding.binding            = 0;
            samplerBinding.descriptorCount    = 1;
            samplerBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerBinding.pImmutableSamplers = nullptr;
            samplerBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo samplerLayoutInfo{};
            samplerLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            samplerLayoutInfo.bindingCount = 1;
            samplerLayoutInfo.pBindings    = &samplerBinding;

            if (vkCreateDescriptorSetLayout(vcsheet::getDevice(), &samplerLayoutInfo, nullptr, &descriptorSetLayouts[2]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create texture descriptor set layout!");
        }

        void createDescriptorPools()
        {
            constexpr uint32_t MAX_VIEWS = 8; // max simultaneous render views (cameras)

            // Camera UBO pool — fixed: MAX_VIEWS sets per frame in flight
            {
                VkDescriptorPoolSize poolSize{};
                poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                poolSize.descriptorCount = framesInFlight * MAX_VIEWS;

                VkDescriptorPoolCreateInfo poolInfo{};
                poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                poolInfo.poolSizeCount = 1;
                poolInfo.pPoolSizes    = &poolSize;
                poolInfo.maxSets       = framesInFlight * MAX_VIEWS;

                if (vkCreateDescriptorPool(vcsheet::getDevice(), &poolInfo, nullptr, &uboDescriptorPool) != VK_SUCCESS)
                    throw std::runtime_error("Failed to create camera UBO descriptor pool!");
            }

            // Object SSBO pool — fixed: one set per frame in flight, shared for all objects
            {
                VkDescriptorPoolSize poolSize{};
                poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                poolSize.descriptorCount = framesInFlight;

                VkDescriptorPoolCreateInfo poolInfo{};
                poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                poolInfo.poolSizeCount = 1;
                poolInfo.pPoolSizes    = &poolSize;
                poolInfo.maxSets       = framesInFlight;

                if (vkCreateDescriptorPool(vcsheet::getDevice(), &poolInfo, nullptr, &ssboDescriptorPool) != VK_SUCCESS)
                    throw std::runtime_error("Failed to create object SSBO descriptor pool!");
            }

            // Sampler pool — scales with unique texture count, not object count
            {
                constexpr uint32_t MAX_TEXTURES = 64;
                VkDescriptorPoolSize poolSize{};
                poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                poolSize.descriptorCount = framesInFlight * MAX_TEXTURES;

                VkDescriptorPoolCreateInfo poolInfo{};
                poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                poolInfo.poolSizeCount = 1;
                poolInfo.pPoolSizes    = &poolSize;
                poolInfo.maxSets       = framesInFlight * MAX_TEXTURES;

                if (vkCreateDescriptorPool(vcsheet::getDevice(), &poolInfo, nullptr, &samplerDescriptorPool) != VK_SUCCESS)
                    throw std::runtime_error("Failed to create sampler descriptor pool!");
            }
        }
};
