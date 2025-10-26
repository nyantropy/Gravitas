#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>
#include <memory>

#include "Vertex.h"

#include "VulkanShader.hpp"
#include "VulkanShaderConfig.h"

#include "VulkanPipelineConfig.h"

#include "dssheet.h"
#include "vcsheet.h"

class VulkanPipeline
{
    public:
        VulkanPipeline(VulkanPipelineConfig& config);
        ~VulkanPipeline();

        VkPipelineLayout& getPipelineLayout();
        VkPipeline& getPipeline();

    private:
        VulkanPipelineConfig config;

        std::unique_ptr<VulkanShader> vertexShader;
        std::unique_ptr<VulkanShader> fragmentShader;
        void createVertexShader();
        void createFragmentShader();

        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline;
        void createGraphicsPipeline();
};