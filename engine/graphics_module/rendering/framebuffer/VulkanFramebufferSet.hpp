#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

#include "vcsheet.h"
#include "VulkanFramebufferSetConfig.h"

// designed as a "set" of framebuffers, since we almost always end up with multiple framebuffers anyway
class VulkanFramebufferSet
{
    public:
        VulkanFramebufferSet(const VulkanFramebufferSetConfig& config);
        ~VulkanFramebufferSet();

        const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers; }

    private:
        std::vector<VkFramebuffer> framebuffers;
        VulkanFramebufferSetConfig config;

        void createFramebuffers();
        void destroyFramebuffers();
};