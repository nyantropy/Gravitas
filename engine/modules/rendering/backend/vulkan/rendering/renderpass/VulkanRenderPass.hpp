#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <stdexcept>

#include "VulkanRenderPassConfig.h"
#include "VulkanBackendContext.h"

// the recipe for rendering, for now we keep it simple
class VulkanRenderPass
{
    private:
        VulkanBackendContext& backendContext;
        VkRenderPass renderPass;
        VulkanRenderPassConfig config;
        void createRenderPass();
    public:
        VulkanRenderPass(VulkanBackendContext& backendContext, VulkanRenderPassConfig config);
        ~VulkanRenderPass();
        VkRenderPass& getRenderPass();
};
