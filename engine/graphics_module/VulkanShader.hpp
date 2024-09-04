#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <fstream>

#include "VulkanLogicalDevice.hpp"

class VulkanShader 
{
    public:
        VulkanShader(VulkanLogicalDevice* vlogicaldevice, const std::string& shaderFile);
        ~VulkanShader();

        VkShaderModule& getShaderModule();

        
    private:
        VkShaderModule shaderModule;

        VulkanLogicalDevice* vlogicaldevice;

        void createShaderModule(const std::vector<char>& code);
};