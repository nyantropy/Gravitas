#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include <Types.h>

struct UniformBufferResource
{
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    std::vector<VkDescriptorSet> descriptorSets;
};