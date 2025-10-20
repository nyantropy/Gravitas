#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

#include "VulkanFramebufferConfig.h"

// designed as a "set" of framebuffers, since we almost always end up with multiple framebuffers anyway
class VulkanFramebufferSet
{
    public:
        VulkanFramebufferSet(const VulkanFramebufferConfig& config);
        ~VulkanFramebufferSet();

        VulkanFramebufferSet(const VulkanFramebufferSet&) = delete;
        VulkanFramebufferSet& operator=(const VulkanFramebufferSet&) = delete;

        VulkanFramebufferSet(VulkanFramebufferSet&& other) noexcept;
        VulkanFramebufferSet& operator=(VulkanFramebufferSet&& other) noexcept;

        const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers; }

    private:
        std::vector<VkFramebuffer> framebuffers;
        VulkanFramebufferConfig config;

        void createFramebuffers();
        void destroyFramebuffers();
};