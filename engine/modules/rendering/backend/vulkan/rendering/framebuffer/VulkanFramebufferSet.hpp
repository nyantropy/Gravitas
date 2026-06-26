#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

#include "VulkanFramebufferSetConfig.h"
#include "VulkanBackendContext.h"

// designed as a "set" of framebuffers, since we almost always end up with multiple framebuffers anyway
class VulkanFramebufferSet
{
    public:
        VulkanFramebufferSet(VulkanBackendContext& backendContext, const VulkanFramebufferSetConfig& config);
        ~VulkanFramebufferSet();

        const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers; }

    private:
        VulkanBackendContext& backendContext;
        std::vector<VkFramebuffer> framebuffers;
        VulkanFramebufferSetConfig config;

        void createFramebuffers();
        void destroyFramebuffers();
};
