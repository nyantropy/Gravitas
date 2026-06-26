#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>
#include <memory>

#include "Vertex.h"

#include "VulkanShader.hpp"
#include "VulkanShaderConfig.h"

#include "VulkanPipelineConfig.h"

#include "DescriptorSetManager.hpp"
#include "VulkanBackendContext.h"

class VulkanPipeline
{
    public:
        VulkanPipeline(VulkanBackendContext& backendContext,
                       DescriptorSetManager& descriptorSetManager,
                       VulkanPipelineConfig& config);
        ~VulkanPipeline();

        VkPipelineLayout& getPipelineLayout();
        VkPipeline& getPipeline();

    private:
        VulkanBackendContext& backendContext;
        DescriptorSetManager& descriptorSetManager;
        VulkanPipelineConfig config;

        std::unique_ptr<VulkanShader> vertexShader;
        std::unique_ptr<VulkanShader> fragmentShader;
        void createVertexShader();
        void createFragmentShader();

        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline;
        void createGraphicsPipeline();
};
