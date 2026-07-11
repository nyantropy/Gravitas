#pragma once

#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>

#include "Types.h"
#include "TextureColorSpace.h"
#include "VulkanTexture.hpp"

struct TextureResource
{
    std::unique_ptr<VulkanTexture> texture;
    std::vector<VkDescriptorSet> descriptorSets;
    std::string cacheKey;
    int width = 0;
    int height = 0;
    texture_id_type id = 0;
    TextureColorSpace colorSpace = TextureColorSpace::SRgb;
};
