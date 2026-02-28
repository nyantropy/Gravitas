#pragma once

#include <memory>
#include <vector>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include "vcsheet.h"
#include "VulkanFramebufferSet.hpp"
#include "FramebufferManagerConfig.h"
#include "VulkanFramebufferSetConfig.h"

class FramebufferManager
{
public:
    FramebufferManager(const FramebufferManagerConfig& config);

    ~FramebufferManager() = default;

    const std::vector<VkFramebuffer>& getFramebuffers() const { return framebuffers->getFramebuffers(); }

private:
    std::unique_ptr<VulkanFramebufferSet> framebuffers;
    FramebufferManagerConfig config;
};