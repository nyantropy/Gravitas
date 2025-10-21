#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

#include "VulkanTexture.hpp"

struct MaterialComponent 
{
    std::string texturePath;
    VulkanTexture* texture = nullptr;
    std::vector<VkDescriptorSet> descriptorSets;
};
