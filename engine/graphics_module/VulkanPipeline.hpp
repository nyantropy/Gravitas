#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Vertex.h"

#include "VulkanShader.hpp"
#include "VulkanLogicalDevice.hpp"
#include "VulkanRenderPass.hpp"
#include "GTSDescriptorSetManager.hpp"

class VulkanPipeline
{
    public:
        VulkanPipeline(VulkanLogicalDevice* vlogicaldevice, GTSDescriptorSetManager* vdescriptorsetmanager, VulkanRenderPass* vrenderpass, std::vector<std::string> shaderPaths);
        ~VulkanPipeline();

        VkPipelineLayout& getPipelineLayout();
        VkPipeline& getPipeline();

    private:
        VulkanLogicalDevice* vlogicaldevice;
        GTSDescriptorSetManager* vdescriptorsetmanager;
        VulkanRenderPass* vrenderpass;

        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline;
        void createGraphicsPipeline(std::string vshaderpath, std::string fshaderpath);
};