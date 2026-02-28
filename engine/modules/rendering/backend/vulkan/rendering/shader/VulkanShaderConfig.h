#pragma once

#include <vulkan/vulkan.h>
#include <string>

struct VulkanShaderConfig
{
    VkDevice vkDevice;
    std::string shaderFile;
};