#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <filesystem>

#include "VulkanShaderConfig.h"

class VulkanShader 
{
    public:
        VulkanShader(VulkanShaderConfig& config);
        ~VulkanShader();

        VkShaderModule& getShaderModule();

        
    private:
        VkShaderModule shaderModule;

        VulkanShaderConfig config;

        void createShaderModule(const std::vector<char>& code);
};