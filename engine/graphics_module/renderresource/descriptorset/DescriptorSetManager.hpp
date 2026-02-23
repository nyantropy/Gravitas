#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <memory>
#include <stdexcept>

#include "vcsheet.h"

// creates 3 descriptor set layouts:
//   set 0 = camera UBO (view, proj)   - VERTEX
//   set 1 = object UBO (model)        - VERTEX
//   set 2 = texture sampler           - FRAGMENT
// this class will sigsegv if the vulkan context is not created before instantiating it
class DescriptorSetManager 
{
    public:
        DescriptorSetManager(uint32_t framesInFlight, uint32_t objectCount): framesInFlight(framesInFlight), objectCount(objectCount)
        {
            createDescriptorSetLayouts();
            createDescriptorPools();
        }

        ~DescriptorSetManager()
        {
            VkDevice device = vcsheet::getDevice();

            if (uboDescriptorPool != VK_NULL_HANDLE)
                vkDestroyDescriptorPool(device, uboDescriptorPool, nullptr);

            if (samplerDescriptorPool != VK_NULL_HANDLE)
                vkDestroyDescriptorPool(device, samplerDescriptorPool, nullptr);

            for (auto layout : descriptorSetLayouts)
                if (layout != VK_NULL_HANDLE)
                    vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }

        const std::array<VkDescriptorSetLayout, 3>& getDescriptorSetLayouts() const { return descriptorSetLayouts; }

        std::vector<VkDescriptorSet> allocateForUniformBuffer(const std::vector<VkBuffer>& uniformBuffers, VkDeviceSize range)
        {
            std::vector<VkDescriptorSet> descriptorSets(framesInFlight);

            // layouts[0] and [1] share the same spec - use either for UBO allocation
            std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayouts[0]);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = uboDescriptorPool;
            allocInfo.descriptorSetCount = framesInFlight;
            allocInfo.pSetLayouts = layouts.data();

            if (vkAllocateDescriptorSets(vcsheet::getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
                throw std::runtime_error("Failed to allocate UBO descriptor sets!");

            for (size_t i = 0; i < framesInFlight; i++) 
            {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = uniformBuffers[i];
                bufferInfo.offset = 0;
                bufferInfo.range = range;

                VkWriteDescriptorSet write{};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = descriptorSets[i];
                write.dstBinding = 0;
                write.dstArrayElement = 0;
                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.descriptorCount = 1;
                write.pBufferInfo = &bufferInfo;

                vkUpdateDescriptorSets(vcsheet::getDevice(), 1, &write, 0, nullptr);
            }

            return descriptorSets;
        }

        std::vector<VkDescriptorSet> allocateForTexture(VkImageView imageView, VkSampler sampler)
        {
            std::vector<VkDescriptorSet> descriptorSets(framesInFlight);

            std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayouts[2]);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = samplerDescriptorPool;
            allocInfo.descriptorSetCount = framesInFlight;
            allocInfo.pSetLayouts = layouts.data();

            if (vkAllocateDescriptorSets(vcsheet::getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
                throw std::runtime_error("Failed to allocate texture descriptor sets!");

            for (size_t i = 0; i < framesInFlight; i++) 
            {
                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = imageView;
                imageInfo.sampler = sampler;

                VkWriteDescriptorSet write{};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = descriptorSets[i];
                write.dstBinding = 0;
                write.dstArrayElement = 0;
                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.descriptorCount = 1;
                write.pImageInfo = &imageInfo;

                vkUpdateDescriptorSets(vcsheet::getDevice(), 1, &write, 0, nullptr);
            }

            return descriptorSets;
        }

    private:
        uint32_t framesInFlight;
        uint32_t objectCount;

        // [0] = camera UBO layout, [1] = object UBO layout, [2] = sampler layout
        // [0] and [1] are separate Vulkan objects with identical specs (binding=0, UNIFORM_BUFFER)
        std::array<VkDescriptorSetLayout, 3> descriptorSetLayouts{};
        VkDescriptorPool uboDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorPool samplerDescriptorPool = VK_NULL_HANDLE;

        void createDescriptorSetLayouts()
        {
            // UBO binding spec (shared by set 0 and set 1)
            VkDescriptorSetLayoutBinding uboBinding{};
            uboBinding.binding = 0;
            uboBinding.descriptorCount = 1;
            uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboBinding.pImmutableSamplers = nullptr;
            uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

            VkDescriptorSetLayoutCreateInfo uboLayoutInfo{};
            uboLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            uboLayoutInfo.bindingCount = 1;
            uboLayoutInfo.pBindings = &uboBinding;

            // set 0: camera UBO layout
            if (vkCreateDescriptorSetLayout(vcsheet::getDevice(), &uboLayoutInfo, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create camera UBO descriptor set layout!");

            // set 1: object UBO layout (identical spec, separate Vulkan object)
            if (vkCreateDescriptorSetLayout(vcsheet::getDevice(), &uboLayoutInfo, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create object UBO descriptor set layout!");

            // set 2: texture sampler layout
            VkDescriptorSetLayoutBinding samplerBinding{};
            samplerBinding.binding = 0;
            samplerBinding.descriptorCount = 1;
            samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerBinding.pImmutableSamplers = nullptr;
            samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo samplerLayoutInfo{};
            samplerLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            samplerLayoutInfo.bindingCount = 1;
            samplerLayoutInfo.pBindings = &samplerBinding;

            if (vkCreateDescriptorSetLayout(vcsheet::getDevice(), &samplerLayoutInfo, nullptr, &descriptorSetLayouts[2]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create texture descriptor set layout!");
        }

        void createDescriptorPools()
        {
            // UBO pool: covers both camera and object UBO allocations
            VkDescriptorPoolSize uboPoolSize{};
            uboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboPoolSize.descriptorCount = framesInFlight * objectCount;

            VkDescriptorPoolCreateInfo uboPoolInfo{};
            uboPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            uboPoolInfo.poolSizeCount = 1;
            uboPoolInfo.pPoolSizes = &uboPoolSize;
            uboPoolInfo.maxSets = framesInFlight * objectCount;

            if (vkCreateDescriptorPool(vcsheet::getDevice(), &uboPoolInfo, nullptr, &uboDescriptorPool) != VK_SUCCESS)
                throw std::runtime_error("Failed to create UBO descriptor pool!");

            // Sampler pool
            VkDescriptorPoolSize samplerPoolSize{};
            samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerPoolSize.descriptorCount = framesInFlight * objectCount;

            VkDescriptorPoolCreateInfo samplerPoolInfo{};
            samplerPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            samplerPoolInfo.poolSizeCount = 1;
            samplerPoolInfo.pPoolSizes = &samplerPoolSize;
            samplerPoolInfo.maxSets = framesInFlight * objectCount;

            if (vkCreateDescriptorPool(vcsheet::getDevice(), &samplerPoolInfo, nullptr, &samplerDescriptorPool) != VK_SUCCESS)
                throw std::runtime_error("Failed to create sampler descriptor pool!");
        }
};
