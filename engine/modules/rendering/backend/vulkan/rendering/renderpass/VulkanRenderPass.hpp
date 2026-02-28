#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <stdexcept>

#include "VulkanRenderPassConfig.h"
#include "vcsheet.h"

// the recipe for rendering, for now we keep it simple
class VulkanRenderPass
{
    private:
        VkRenderPass renderPass;
        VulkanRenderPassConfig config;
        void createRenderPass();
    public:
        VulkanRenderPass(VulkanRenderPassConfig config);
        ~VulkanRenderPass();
        VkRenderPass& getRenderPass();
};
