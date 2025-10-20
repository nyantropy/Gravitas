#pragma once

#include <memory>
#include <vector>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include "VulkanFramebufferSet.hpp"
#include "VulkanFramebufferManagerConfig.h"

class VulkanFramebufferManager
{
public:
    VulkanFramebufferManager(const VulkanFramebufferManagerConfig& config);

    ~VulkanFramebufferManager() = default;

    const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers->getFramebuffers(); }

private:
    std::unique_ptr<VulkanFramebufferSet> framebuffers;
    VulkanFramebufferManagerConfig config;
};