#pragma once

#include <memory>
#include <vector>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include "VulkanFramebufferSet.hpp"
#include "FramebufferManagerConfig.h"
#include "VulkanFramebufferSetConfig.h"
#include "VulkanBackendContext.h"

class FramebufferManager
{
public:
    FramebufferManager(VulkanBackendContext& backendContext, const FramebufferManagerConfig& config);

    ~FramebufferManager() = default;

    const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers->getFramebuffers(); }

private:
    VulkanBackendContext& backendContext;
    std::unique_ptr<VulkanFramebufferSet> framebuffers;
    FramebufferManagerConfig config;
};
