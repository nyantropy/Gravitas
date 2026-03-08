#pragma once

#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

#include "VulkanTexture.hpp"

struct TextureResource
{
    std::unique_ptr<VulkanTexture> texture;
    std::vector<VkDescriptorSet> descriptorSets;
};