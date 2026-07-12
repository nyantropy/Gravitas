#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <memory>
#include <stdexcept>

#include "VulkanBackendContext.h"

// Creates 4 descriptor set layouts:
//   set 0 = camera/frame UBO (camera, light)  - VERTEX+FRAGMENT, UNIFORM_BUFFER
//   set 1 = object SSBO (array of ObjectData) - VERTEX, STORAGE_BUFFER
//   set 2 = material texture samplers         - FRAGMENT, COMBINED_IMAGE_SAMPLER
//   set 3 = environment IBL samplers          - FRAGMENT, COMBINED_IMAGE_SAMPLER
//
// Pools are fixed-size and never scale with object count.
class DescriptorSetManager
{
    public:
        static constexpr uint32_t MaterialTextureBindingCount = 5;
        static constexpr uint32_t EnvironmentTextureBindingCount = 3;

        explicit DescriptorSetManager(VulkanBackendContext& backendContext, uint32_t framesInFlight)
            : backendContext(backendContext), framesInFlight(framesInFlight)
        {
            createDescriptorSetLayouts();
            createDescriptorPools();
        }

        ~DescriptorSetManager()
        {
            VkDevice device = backendContext.device();

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

        const std::array<VkDescriptorSetLayout, 4>& getDescriptorSetLayouts() const { return descriptorSetLayouts; }

        // Frees camera UBO descriptor sets back to the UBO pool.
        // The pool must have been created with VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT.
        void freeUniformBufferSets(const std::vector<VkDescriptorSet>& sets)
        {
            if (sets.empty()) return;
            vkFreeDescriptorSets(backendContext.device(), uboDescriptorPool,
                                 static_cast<uint32_t>(sets.size()), sets.data());
        }

        void freeSampledImageSets(const std::vector<VkDescriptorSet>& sets)
        {
            if (sets.empty()) return;
            vkFreeDescriptorSets(backendContext.device(), samplerDescriptorPool,
                                 static_cast<uint32_t>(sets.size()), sets.data());
        }

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

            if (vkAllocateDescriptorSets(backendContext.device(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
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

                vkUpdateDescriptorSets(backendContext.device(), 1, &write, 0, nullptr);
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

            if (vkAllocateDescriptorSets(backendContext.device(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
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

                vkUpdateDescriptorSets(backendContext.device(), 1, &write, 0, nullptr);
            }

            return descriptorSets;
        }

        std::vector<VkDescriptorSet> allocateForTexture(VkImageView imageView, VkSampler sampler)
        {
            return allocateForSampledImage(imageView, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        std::vector<VkDescriptorSet> allocateForSampledImage(VkImageView imageView,
                                                             VkSampler sampler,
                                                             VkImageLayout imageLayout)
        {
            std::vector<VkDescriptorSet> descriptorSets(framesInFlight);

            std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayouts[2]);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool     = samplerDescriptorPool;
            allocInfo.descriptorSetCount = framesInFlight;
            allocInfo.pSetLayouts        = layouts.data();

            if (vkAllocateDescriptorSets(backendContext.device(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
                throw std::runtime_error("Failed to allocate texture descriptor sets!");

            for (size_t i = 0; i < framesInFlight; i++)
            {
                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = imageLayout;
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

                vkUpdateDescriptorSets(backendContext.device(), 1, &write, 0, nullptr);
            }

            return descriptorSets;
        }

        std::vector<VkDescriptorSet> allocateForMaterialTextures(
            const std::array<VkDescriptorImageInfo, MaterialTextureBindingCount>& images)
        {
            std::vector<VkDescriptorSet> descriptorSets(framesInFlight);

            std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayouts[2]);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool     = samplerDescriptorPool;
            allocInfo.descriptorSetCount = framesInFlight;
            allocInfo.pSetLayouts        = layouts.data();

            if (vkAllocateDescriptorSets(backendContext.device(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
                throw std::runtime_error("Failed to allocate material texture descriptor sets!");

            for (size_t frame = 0; frame < framesInFlight; ++frame)
            {
                std::array<VkWriteDescriptorSet, MaterialTextureBindingCount> writes{};
                for (uint32_t binding = 0; binding < MaterialTextureBindingCount; ++binding)
                {
                    writes[binding].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writes[binding].dstSet          = descriptorSets[frame];
                    writes[binding].dstBinding      = binding;
                    writes[binding].dstArrayElement = 0;
                    writes[binding].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writes[binding].descriptorCount = 1;
                    writes[binding].pImageInfo      = &images[binding];
                }

                vkUpdateDescriptorSets(
                    backendContext.device(),
                    static_cast<uint32_t>(writes.size()),
                    writes.data(),
                    0,
                    nullptr);
            }

            return descriptorSets;
        }

        std::vector<VkDescriptorSet> allocateForEnvironmentTextures(
            const std::array<VkDescriptorImageInfo, EnvironmentTextureBindingCount>& images)
        {
            std::vector<VkDescriptorSet> descriptorSets(framesInFlight);

            std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayouts[3]);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool     = samplerDescriptorPool;
            allocInfo.descriptorSetCount = framesInFlight;
            allocInfo.pSetLayouts        = layouts.data();

            if (vkAllocateDescriptorSets(backendContext.device(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
                throw std::runtime_error("Failed to allocate environment texture descriptor sets!");

            for (size_t frame = 0; frame < framesInFlight; ++frame)
            {
                std::array<VkWriteDescriptorSet, EnvironmentTextureBindingCount> writes{};
                for (uint32_t binding = 0; binding < EnvironmentTextureBindingCount; ++binding)
                {
                    writes[binding].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writes[binding].dstSet          = descriptorSets[frame];
                    writes[binding].dstBinding      = binding;
                    writes[binding].dstArrayElement = 0;
                    writes[binding].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writes[binding].descriptorCount = 1;
                    writes[binding].pImageInfo      = &images[binding];
                }

                vkUpdateDescriptorSets(
                    backendContext.device(),
                    static_cast<uint32_t>(writes.size()),
                    writes.data(),
                    0,
                    nullptr);
            }

            return descriptorSets;
        }

    private:
        VulkanBackendContext& backendContext;
        uint32_t framesInFlight;

        // [0] = camera UBO, [1] = object SSBO, [2] = material samplers, [3] = environment samplers
        std::array<VkDescriptorSetLayout, 4> descriptorSetLayouts{};

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
            uboBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo uboLayoutInfo{};
            uboLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            uboLayoutInfo.bindingCount = 1;
            uboLayoutInfo.pBindings    = &uboBinding;

            if (vkCreateDescriptorSetLayout(backendContext.device(), &uboLayoutInfo, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS)
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

            if (vkCreateDescriptorSetLayout(backendContext.device(), &ssboLayoutInfo, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create object SSBO descriptor set layout!");

            // set 2: material texture samplers. Binding 0 is the base-color
            // sampler used by unlit, UI, and particle shaders.
            std::array<VkDescriptorSetLayoutBinding, MaterialTextureBindingCount> samplerBindings{};
            for (uint32_t binding = 0; binding < MaterialTextureBindingCount; ++binding)
            {
                samplerBindings[binding].binding            = binding;
                samplerBindings[binding].descriptorCount    = 1;
                samplerBindings[binding].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                samplerBindings[binding].pImmutableSamplers = nullptr;
                samplerBindings[binding].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            VkDescriptorSetLayoutCreateInfo samplerLayoutInfo{};
            samplerLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            samplerLayoutInfo.bindingCount = static_cast<uint32_t>(samplerBindings.size());
            samplerLayoutInfo.pBindings    = samplerBindings.data();

            if (vkCreateDescriptorSetLayout(backendContext.device(), &samplerLayoutInfo, nullptr, &descriptorSetLayouts[2]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create texture descriptor set layout!");

            // set 3: environment IBL samplers.
            std::array<VkDescriptorSetLayoutBinding, EnvironmentTextureBindingCount> environmentBindings{};
            for (uint32_t binding = 0; binding < EnvironmentTextureBindingCount; ++binding)
            {
                environmentBindings[binding].binding            = binding;
                environmentBindings[binding].descriptorCount    = 1;
                environmentBindings[binding].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                environmentBindings[binding].pImmutableSamplers = nullptr;
                environmentBindings[binding].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            VkDescriptorSetLayoutCreateInfo environmentLayoutInfo{};
            environmentLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            environmentLayoutInfo.bindingCount = static_cast<uint32_t>(environmentBindings.size());
            environmentLayoutInfo.pBindings    = environmentBindings.data();

            if (vkCreateDescriptorSetLayout(backendContext.device(), &environmentLayoutInfo, nullptr, &descriptorSetLayouts[3]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create environment descriptor set layout!");
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
                // FREE_DESCRIPTOR_SET_BIT required to call vkFreeDescriptorSets on
                // individual camera views when a scene is reloaded mid-session.
                poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
                poolInfo.poolSizeCount = 1;
                poolInfo.pPoolSizes    = &poolSize;
                poolInfo.maxSets       = framesInFlight * MAX_VIEWS;

                if (vkCreateDescriptorPool(backendContext.device(), &poolInfo, nullptr, &uboDescriptorPool) != VK_SUCCESS)
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

                if (vkCreateDescriptorPool(backendContext.device(), &poolInfo, nullptr, &ssboDescriptorPool) != VK_SUCCESS)
                    throw std::runtime_error("Failed to create object SSBO descriptor pool!");
            }

            // Sampler pool — scales with unique texture/material/environment
            // descriptor sets, not object count.
            {
                constexpr uint32_t MAX_TEXTURE_DESCRIPTOR_SETS = 512;
                VkDescriptorPoolSize poolSize{};
                poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                poolSize.descriptorCount =
                    framesInFlight * MAX_TEXTURE_DESCRIPTOR_SETS *
                    (MaterialTextureBindingCount + EnvironmentTextureBindingCount);

                VkDescriptorPoolCreateInfo poolInfo{};
                poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
                poolInfo.poolSizeCount = 1;
                poolInfo.pPoolSizes    = &poolSize;
                poolInfo.maxSets       = framesInFlight * MAX_TEXTURE_DESCRIPTOR_SETS;

                if (vkCreateDescriptorPool(backendContext.device(), &poolInfo, nullptr, &samplerDescriptorPool) != VK_SUCCESS)
                    throw std::runtime_error("Failed to create sampler descriptor pool!");
            }
        }
};
